//===- Alpha.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Alpha is a GP-relative architecture. Every function establishes a global
// pointer in $29 (gp) with an ldah/lda pair covered by a single R_ALPHA_GPDISP
// relocation, and then reaches static data and GOT entries with 16-bit
// displacements from gp. By convention gp points 0x8000 bytes past the start
// of .got, so a single GOT of up to 64KB is addressable with a signed 16-bit
// displacement.
//
//===----------------------------------------------------------------------===//

#include "InputFiles.h"
#include "RelocScan.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

// st_other bits describing a function's gp-load prologue. A !samegp caller
// may skip the two-instruction gp load of a STO_ALPHA_STD_GPLOAD callee.
enum { STO_ALPHA_NOPV = 0x80, STO_ALPHA_STD_GPLOAD = 0x88 };

// The kinds of value a GOT entry can hold. Two kinds for the same symbol are
// distinct entries.
enum GotKind : uint32_t {
  GK_Addr,     // the symbol's address (R_ALPHA_LITERAL)
  GK_TpOff,    // the symbol's thread-pointer offset (R_ALPHA_GOTTPREL)
  GK_DynTls,   // module index and dtp offset pair (R_ALPHA_TLSGD)
  GK_TlsIndex, // module index and zero pair (R_ALPHA_TLSLDM)
  GK_Max
};

// A GOT entry is identified by the symbol, the addend (the assembler turns
// references to local symbols into a section symbol plus an addend, so
// R_ALPHA_LITERAL routinely needs an entry holding S + A), and the kind of
// value it holds.
using GotKey = std::pair<Symbol *, std::pair<int64_t, uint32_t>>;

namespace {
class Alpha final : public TargetInfo {
public:
  Alpha(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  RelType getDynRel(RelType type) const override;
  void finalizeRelocScan() override;
  void scanSection(InputSectionBase &sec, unsigned shard) override;
  template <class ELFT, class RelTy>
  void scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels,
                       unsigned shard);
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;

private:
  uint64_t allocGot(unsigned slots);
  uint64_t getGotEntry(Symbol &sym, int64_t addend, GotKind kind);

  // gp is defined to be 0x8000 bytes past the start of .got, so it reaches
  // [gp - 32768, gp + 32767] and no further.
  static constexpr uint64_t gpBias = 0x8000;

  llvm::DenseMap<GotKey, uint64_t> gotEntries;
  uint64_t gotSize = 0;
};
} // namespace

Alpha::Alpha(Ctx &ctx) : TargetInfo(ctx) {
  copyRel = R_ALPHA_COPY;
  gotRel = R_ALPHA_GLOB_DAT;
  pltRel = R_ALPHA_JMP_SLOT;
  relativeRel = R_ALPHA_RELATIVE;
  symbolicRel = R_ALPHA_REFQUAD;
  tlsModuleIndexRel = R_ALPHA_DTPMOD64;
  tlsOffsetRel = R_ALPHA_DTPREL64;
  tlsGotRel = R_ALPHA_TPREL64;
  gotEntrySize = 8;

  // Alpha Linux runs with 8KB pages, but the ABI reserves 64KB of alignment
  // between segments, matching bfd's ELF_MAXPAGESIZE for elf64-alpha.
  defaultCommonPageSize = 8192;
  defaultMaxPageSize = 0x10000;
  defaultImageBase = 0x120000000;
}

RelExpr Alpha::getRelExpr(RelType type, const Symbol &s,
                          const uint8_t *loc) const {
  switch (type) {
  case R_ALPHA_NONE:
  // R_ALPHA_LITUSE only annotates the instruction that consumes a preceding
  // R_ALPHA_LITERAL. It is used by --relax, which we do not implement, and
  // never contributes a value.
  case R_ALPHA_LITUSE:
    return R_NONE;
  case R_ALPHA_REFLONG:
  case R_ALPHA_REFQUAD:
    return R_ABS;
  case R_ALPHA_SREL16:
  case R_ALPHA_SREL32:
  case R_ALPHA_SREL64:
    return R_PC;
  case R_ALPHA_BRADDR:
  case R_ALPHA_BRSGP:
    return R_PLT_PC;
  case R_ALPHA_HINT:
    // The hint only steers the branch predictor for an indirect jsr; a wrong
    // value costs performance, never correctness. A call to a preemptible
    // symbol is always out of range, so leave the field alone, as bfd does.
    return s.isPreemptible ? R_NONE : R_PC;
  case R_ALPHA_GPREL16:
  case R_ALPHA_GPREL32:
  case R_ALPHA_GPRELHIGH:
  case R_ALPHA_GPRELLOW:
    return RE_ALPHA_GPREL;
  case R_ALPHA_GPDISP:
    return RE_ALPHA_GPDISP;
  // The relocations that consume a GOT entry are handled entirely by
  // scanSectionImpl, which allocates the entry and records its offset. They
  // cannot be resolved from a bare type and symbol, so if one turns up in a
  // section that is not scanned (.eh_frame or a non-alloc section) there is
  // nothing sensible to compute.
  case R_ALPHA_LITERAL:
  case R_ALPHA_TLSGD:
  case R_ALPHA_TLSLDM:
  case R_ALPHA_GOTTPREL:
    return R_NONE;
  case R_ALPHA_DTPREL64:
  case R_ALPHA_DTPRELHI:
  case R_ALPHA_DTPRELLO:
  case R_ALPHA_DTPREL16:
    return R_DTPREL;
  case R_ALPHA_TPREL64:
  case R_ALPHA_TPRELHI:
  case R_ALPHA_TPRELLO:
  case R_ALPHA_TPREL16:
    return R_TPREL;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

RelType Alpha::getDynRel(RelType type) const {
  if (type == R_ALPHA_REFQUAD)
    return type;
  return R_ALPHA_NONE;
}

uint64_t Alpha::allocGot(unsigned slots) {
  uint64_t off = gotSize;
  for (unsigned i = 0; i != slots; ++i)
    ctx.in.got->reserveEntry();
  gotSize += 8 * slots;
  return off;
}

// The entry a relocation reads, allocated and initialized on first use. Alpha
// owns its GOT rather than going through NEEDS_GOT and NEEDS_TLS*, because the
// generic GOT is keyed by symbol alone and cannot represent the symbol+addend
// entry that gas asks for whenever it rewrites a reference to a local symbol as
// a section symbol plus an addend. Every kind is keyed on the addend, as bfd's
// alpha_elf_got_entry keys all of them, so `ldq $1, x+8($29) !gottprel' reaches
// the offset of x+8 rather than of x. Relocation scanning is serialized for
// Alpha, so no locking is needed here.
uint64_t Alpha::getGotEntry(Symbol &sym, int64_t addend, GotKind kind) {
  GotKey key{&sym, {addend, kind}};
  auto [it, inserted] = gotEntries.try_emplace(key, 0);
  if (!inserted)
    return it->second;

  GotSection &got = *ctx.in.got;
  uint64_t off = allocGot(kind == GK_DynTls || kind == GK_TlsIndex ? 2 : 1);
  it->second = off;

  switch (kind) {
  case GK_Addr:
    got.addConstant({R_ABS, symbolicRel, off, addend, &sym});
    break;
  case GK_TpOff:
    got.addConstant({R_TPREL, symbolicRel, off, addend, &sym});
    break;
  case GK_DynTls:
    // The module index, then the offset of the symbol within that module's TLS
    // block. In an executable the module index is always 1.
    got.addConstant({R_ADDEND, symbolicRel, off, 1, &sym});
    got.addConstant({R_ABS, tlsOffsetRel, off + 8, addend, &sym});
    break;
  case GK_TlsIndex:
    // Only the module index matters; the second slot stays zero and the caller
    // adds the symbol's dtp offset with R_ALPHA_DTPREL*.
    got.addConstant({R_ADDEND, symbolicRel, off, 1, ctx.dummySym});
    break;
  case GK_Max:
    llvm_unreachable("not a GOT entry kind");
  }
  return off;
}

// Alpha keeps all TLS GOT slots addressed through gp, and its GD/LD sequences
// call __tls_get_addr through a separate R_ALPHA_LITERAL, so a GD or LD
// relocation cannot be rewritten into IE or LE without also rewriting the
// call. No TLS optimization is performed; every model uses its own GOT slots.
template <class ELFT, class RelTy>
void Alpha::scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels,
                            unsigned shard) {
  RelocScan rs(ctx, &sec, shard);
  sec.relocations.reserve(rels.size());

  for (auto it = rels.begin(); it != rels.end(); ++it) {
    RelType type = it->getType(false);
    uint32_t symIdx = it->getSymbol(false);
    Symbol &sym = sec.getFile<ELFT>()->getSymbol(symIdx);
    uint64_t offset = it->r_offset;
    if (sym.isUndefined() && symIdx != 0 &&
        rs.maybeReportUndefined(cast<Undefined>(sym), offset))
      continue;
    int64_t addend = rs.getAddend<ELFT>(*it, type);

    switch (type) {
    // These consume a GOT entry. Allocate it now and carry its offset within
    // .got in the addend; RE_ALPHA_GOT turns that into a gp displacement.
    case R_ALPHA_LITERAL:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_Addr)), &sym});
      continue;
    case R_ALPHA_GOTTPREL:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_TpOff)), &sym});
      continue;
    case R_ALPHA_TLSGD:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_DynTls)), &sym});
      continue;
    case R_ALPHA_TLSLDM:
      // The symbol of a TLSLDM relocation is ignored: the result is always the
      // current module, so one entry answers for all of them.
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(*ctx.dummySym, 0, GK_TlsIndex)), &sym});
      continue;
    case R_ALPHA_TPREL64:
    case R_ALPHA_TPRELHI:
    case R_ALPHA_TPRELLO:
    case R_ALPHA_TPREL16:
      if (rs.checkTlsLe(offset, sym, type))
        continue;
      sec.addReloc({R_TPREL, type, offset, addend, &sym});
      continue;
    case R_ALPHA_DTPREL64:
    case R_ALPHA_DTPRELHI:
    case R_ALPHA_DTPRELLO:
    case R_ALPHA_DTPREL16:
      sec.addReloc({R_DTPREL, type, offset, addend, &sym});
      continue;
    case R_ALPHA_GOTDTPREL:
      Err(ctx) << getErrorLoc(ctx, sec.content().data() + offset)
               << "R_ALPHA_GOTDTPREL is not supported";
      continue;
    default:
      break;
    }

    RelExpr expr = getRelExpr(type, sym, sec.content().data() + offset);
    if (expr == R_NONE)
      continue;
    rs.process(expr, type, offset, sym, addend);
  }
}

void Alpha::scanSection(InputSectionBase &sec, unsigned shard) {
  elf::scanSection1<Alpha, ELF64LE>(*this, sec, shard);
}

void Alpha::finalizeRelocScan() {
  // Every function establishes gp from .got, so gp has to be well defined even
  // in a link that needs no GOT entries. Keep .got from being discarded.
  ctx.in.got->hasGotOffRel.store(true, std::memory_order_relaxed);

  // The PLT and the dynamic relocation forms are not implemented yet, so
  // refuse to emit output that would need them rather than emit it wrong.
  if (ctx.arg.isPic || !ctx.sharedFiles.empty())
    Err(ctx) << "Alpha does not support dynamic linking yet";
}

// Patch the 16-bit immediate field of a single instruction.
static void writeImm16(uint8_t *loc, uint64_t val) {
  write32le(loc, (read32le(loc) & 0xffff0000) | (val & 0xffff));
}

// R_ALPHA_GPDISP covers the ldah/lda pair that materializes gp. The relocation
// is placed on the ldah and its addend is the byte distance to the paired lda
// (normally 4, but 8 when an instruction such as call_pal sits between them).
// Both instructions have to be patched from one relocation, and any immediates
// already present are treated as an addend.
static void relocateGpDisp(Ctx &ctx, uint8_t *loc, const Relocation &rel,
                           uint64_t val) {
  uint8_t *ldahLoc = loc;
  uint8_t *ldaLoc = loc + rel.addend;
  uint32_t ldah = read32le(ldahLoc);
  uint32_t lda = read32le(ldaLoc);

  if ((ldah >> 26) != 0x09 || (lda >> 26) != 0x08) {
    Err(ctx) << getErrorLoc(ctx, loc)
             << "R_ALPHA_GPDISP does not point to an ldah/lda pair";
    return;
  }

  // Recover the in-place addend, undoing the sign extension that each of the
  // two instructions performs on its own immediate.
  uint64_t inplace = (uint64_t(ldah & 0xffff) << 16) | (lda & 0xffff);
  inplace = (inplace ^ 0x80008000) - 0x80008000;

  uint64_t disp = val + inplace;
  if (int64_t(disp) < -int64_t(0x80000000) || int64_t(disp) >= 0x7fff8000) {
    Err(ctx) << getErrorLoc(ctx, loc) << "gp displacement out of range";
    return;
  }

  // The lda sign-extends its immediate, so the ldah half carries the borrow.
  writeImm16(ldahLoc, (disp >> 16) + ((disp >> 15) & 1));
  writeImm16(ldaLoc, disp);
}

void Alpha::relocate(uint8_t *loc, const Relocation &rel, uint64_t val) const {
  switch (rel.type) {
  case R_ALPHA_NONE:
  case R_ALPHA_LITUSE:
    break;
  case R_ALPHA_REFLONG:
    checkIntUInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    break;
  case R_ALPHA_REFQUAD:
    write64le(loc, val);
    break;
  case R_ALPHA_SREL16:
    checkInt(ctx, loc, val, 16, rel);
    write16le(loc, val);
    break;
  case R_ALPHA_SREL32:
    checkInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    break;
  case R_ALPHA_SREL64:
    write64le(loc, val);
    break;
  case R_ALPHA_BRADDR:
  case R_ALPHA_BRSGP: {
    // Alpha branch displacements are measured from the instruction following
    // the branch.
    int64_t disp = int64_t(val) - 4;
    // A !samegp call lands after the callee's gp-load prologue, whose length
    // the callee advertises in st_other. The callee must say which it is:
    // branching into a function that does set up its own gp from $27, but
    // skipping the instructions that do it, leaves gp wrong. bfd rejects this
    // rather than guess, and so do we -- the input is hand-written assembly
    // that forgot .prologue or .usepv.
    if (rel.type == R_ALPHA_BRSGP && rel.sym) {
      switch (rel.sym->stOther & STO_ALPHA_STD_GPLOAD) {
      case STO_ALPHA_NOPV:
        break;
      case STO_ALPHA_STD_GPLOAD:
        disp += 8;
        break;
      default:
        Err(ctx) << getErrorLoc(ctx, loc)
                 << "!samegp reloc against symbol without .prologue: "
                 << rel.sym;
        return;
      }
    }
    checkAlignment(ctx, loc, disp, 4, rel);
    checkInt(ctx, loc, disp, 23, rel);
    write32le(loc, (read32le(loc) & ~0x1fffff) | ((disp >> 2) & 0x1fffff));
    break;
  }
  case R_ALPHA_HINT: {
    int64_t disp = int64_t(val) - 4;
    write32le(loc, (read32le(loc) & ~0x3fff) | ((disp >> 2) & 0x3fff));
    break;
  }
  case R_ALPHA_GPREL16:
    checkInt(ctx, loc, val, 16, rel);
    writeImm16(loc, val);
    break;
  case R_ALPHA_GPREL32:
    checkInt(ctx, loc, val, 32, rel);
    write32le(loc, val);
    break;
  case R_ALPHA_GPRELHIGH:
    writeImm16(loc, (val + 0x8000) >> 16);
    break;
  case R_ALPHA_GPRELLOW:
    writeImm16(loc, val);
    break;
  // The GOT entry's displacement from gp. One gp reaches 64KB of GOT, and
  // nothing here arranges for a second one yet.
  case R_ALPHA_LITERAL:
  case R_ALPHA_TLSGD:
  case R_ALPHA_TLSLDM:
  case R_ALPHA_GOTTPREL:
    if (!isInt<16>(val)) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "GOT displacement out of range; multi-GOT is not implemented";
      break;
    }
    writeImm16(loc, val);
    break;
  case R_ALPHA_GPDISP:
    relocateGpDisp(ctx, loc, rel, val);
    break;
  // The DTPREL/TPREL offsets are plain values, split the same way as GPREL.
  case R_ALPHA_DTPREL16:
  case R_ALPHA_TPREL16:
    checkInt(ctx, loc, val, 16, rel);
    writeImm16(loc, val);
    break;
  case R_ALPHA_DTPRELHI:
  case R_ALPHA_TPRELHI:
    writeImm16(loc, (val + 0x8000) >> 16);
    break;
  case R_ALPHA_DTPRELLO:
  case R_ALPHA_TPRELLO:
    writeImm16(loc, val);
    break;
  case R_ALPHA_DTPREL64:
  case R_ALPHA_TPREL64:
  // Written into GOT slots and dynamic relocation targets.
  case R_ALPHA_DTPMOD64:
  case R_ALPHA_GLOB_DAT:
  case R_ALPHA_JMP_SLOT:
  case R_ALPHA_RELATIVE:
    write64le(loc, val);
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}

void elf::setAlphaTargetInfo(Ctx &ctx) { ctx.target.reset(new Alpha(ctx)); }
