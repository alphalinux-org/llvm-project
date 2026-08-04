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
// A call also goes through the GOT: the caller loads the callee's address with
// R_ALPHA_LITERAL and jumps to it. The PLT therefore exists only to support
// lazy binding -- R_ALPHA_JMP_SLOT relocates the .got slot rather than a
// .got.plt slot, and the PLT stub is what the slot points at until the first
// call resolves it. We bind eagerly and emit no PLT at all, so a preemptible
// callee simply gets R_ALPHA_GLOB_DAT on its GOT entry.
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
  uint64_t getLiteralGotOffset(Symbol &sym, int64_t addend);

  // gp is defined to be 0x8000 bytes past the start of .got.
  static constexpr uint64_t gpBias = 0x8000;

  // GOT entries for R_ALPHA_LITERAL against symbol+addend, which the generic
  // per-symbol GOT cannot represent. Maps the pair to its offset in .got.
  llvm::DenseMap<std::pair<Symbol *, int64_t>, uint64_t> literalGot;
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

  // On Alpha a call loads the callee's address from the ordinary GOT with
  // R_ALPHA_LITERAL and jumps to it, so the PLT exists only as a lazy-binding
  // trampoline: R_ALPHA_JMP_SLOT relocates the .got slot, not a .got.plt slot.
  // We bind eagerly instead and emit no PLT at all, which needs no .got.plt.
  gotPltHeaderEntriesNum = 0;

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
    // There is no PLT (see below), so a branch cannot reach a preemptible
    // symbol. process() reports that as an error.
    return R_PC;
  case R_ALPHA_HINT:
    // The hint only steers the branch predictor for an indirect jsr; a wrong
    // value costs performance, never correctness. A call to a preemptible
    // symbol is always out of range, so leave the field alone, as bfd does.
    return s.isPreemptible ? R_NONE : R_PC;
  case R_ALPHA_GPREL16:
  case R_ALPHA_GPREL32:
  case R_ALPHA_GPRELHIGH:
  case R_ALPHA_GPRELLOW:
    // S + A - gp. R_GOTREL computes S + A - .got; relocate() applies the
    // 0x8000 bias that separates .got from gp.
    return R_GOTREL;
  case R_ALPHA_LITERAL:
    // The GOT entry's displacement from gp. R_GOT_OFF computes the offset
    // from the start of .got; relocate() applies the bias.
    return R_GOT_OFF;
  case R_ALPHA_GPDISP:
    return RE_ALPHA_GPDISP;
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
  case R_ALPHA_TLSGD:
    return R_TLSGD_GOT;
  case R_ALPHA_TLSLDM:
    return R_TLSLD_GOT;
  case R_ALPHA_GOTTPREL:
    return R_GOT_OFF;
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

// gas turns a reference to a local symbol into a section symbol plus an
// addend, so R_ALPHA_LITERAL commonly needs a GOT entry holding S + A. Reserve
// one entry per distinct pair and initialize it like bfd does. Relocation
// scanning is serialized for Alpha, so no locking is needed here.
uint64_t Alpha::getLiteralGotOffset(Symbol &sym, int64_t addend) {
  auto [it, inserted] = literalGot.try_emplace({&sym, addend}, 0);
  if (inserted) {
    it->second = ctx.in.got->reserveEntry();
    // A preemptible symbol's address is only known at run time, and no dynamic
    // relocation can express symbol+addend, which is why bfd only ever forms
    // these entries for local symbols.
    if (sym.isPreemptible)
      Err(ctx) << "R_ALPHA_LITERAL against preemptible symbol '" << &sym
               << "' with a non-zero addend";
    else if (ctx.arg.isPic)
      ctx.in.relaDyn->addRelativeReloc(relativeRel, *ctx.in.got, it->second,
                                       sym, addend, R_ALPHA_REFQUAD, R_ABS);
    else
      ctx.in.got->addConstant(
          {R_ABS, R_ALPHA_REFQUAD, it->second, addend, &sym});
  }
  return it->second;
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
    case R_ALPHA_LITERAL:
      if (addend == 0)
        break;
      // The GOT offset is known now, so carry it in the addend and let
      // relocate() turn it into a gp-relative displacement.
      sec.addReloc({R_ADDEND, type, offset,
                    int64_t(getLiteralGotOffset(sym, addend)), &sym});
      continue;
    case R_ALPHA_TLSGD:
      sym.setFlags(NEEDS_TLSGD);
      sec.addReloc({R_TLSGD_GOT, type, offset, addend, &sym});
      continue;
    case R_ALPHA_TLSLDM:
      ctx.needsTlsLd.store(true, std::memory_order_relaxed);
      sec.addReloc({R_TLSLD_GOT, type, offset, addend, &sym});
      continue;
    case R_ALPHA_GOTTPREL:
      rs.handleTlsIe<false>(R_GOT_OFF, type, offset, addend, sym);
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
  case R_ALPHA_GPREL16: {
    uint64_t v = val - gpBias;
    checkInt(ctx, loc, v, 16, rel);
    writeImm16(loc, v);
    break;
  }
  case R_ALPHA_GPREL32: {
    uint64_t v = val - gpBias;
    checkInt(ctx, loc, v, 32, rel);
    write32le(loc, v);
    break;
  }
  case R_ALPHA_GPRELHIGH: {
    uint64_t v = val - gpBias;
    writeImm16(loc, (v + 0x8000) >> 16);
    break;
  }
  case R_ALPHA_GPRELLOW:
    writeImm16(loc, val - gpBias);
    break;
  case R_ALPHA_LITERAL: {
    uint64_t v = val - gpBias;
    if (!isInt<16>(v)) {
      Err(ctx) << getErrorLoc(ctx, loc)
               << "GOT displacement out of range; multi-GOT is not implemented";
      break;
    }
    writeImm16(loc, v);
    break;
  }
  case R_ALPHA_GPDISP:
    relocateGpDisp(ctx, loc, rel, val);
    break;
  // TLS. The GOT-slot displacements are gp-relative like R_ALPHA_LITERAL; the
  // DTPREL/TPREL offsets are plain values split the same way as GPREL.
  case R_ALPHA_TLSGD:
  case R_ALPHA_TLSLDM:
  case R_ALPHA_GOTTPREL: {
    uint64_t v = val - gpBias;
    checkInt(ctx, loc, v, 16, rel);
    writeImm16(loc, v);
    break;
  }
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
