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
// Because the displacement is only 16 bits, one gp reaches just 64KB of GOT.
// Larger links therefore need more than one gp: input files are grouped into
// partitions, each partition gets its own region of .got and its own gp, and a
// symbol referenced from two partitions gets an entry in each. This mirrors
// bfd's multi-GOT: bfd assigns a GOT per input file and then merges adjacent
// ones while they fit, and we do the same thing in one pass, measuring each
// file as we reach it and starting a new partition when it will not fit in what
// is left of the current one.
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
  GK_DtpOff,   // the symbol's module-relative offset (R_ALPHA_GOTDTPREL)
  GK_DynTls,   // module index and dtp offset pair (R_ALPHA_TLSGD)
  GK_TlsIndex, // module index and zero pair (R_ALPHA_TLSLDM)
  GK_Max
};

// A GOT entry is identified by the symbol, the addend (the assembler turns
// references to local symbols into a section symbol plus an addend, so
// R_ALPHA_LITERAL routinely needs an entry holding S + A), the kind of value it
// holds, and the partition it belongs to.
using GotKey = std::pair<Symbol *, std::pair<int64_t, uint32_t>>;

namespace {
class Alpha final : public TargetInfo {
public:
  Alpha(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  RelType getDynRel(RelType type) const override;
  int64_t getImplicitAddend(const uint8_t *buf, RelType type) const override;
  void finalizeRelocScan() override;
  void scanSection(InputSectionBase &sec, unsigned shard) override;
  template <class ELFT, class RelTy>
  void scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels,
                       unsigned shard);
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;

  uint64_t getGp(const InputFile *f) const override;

private:
  unsigned gotDemand(InputFile *f) const;
  uint64_t allocGot(unsigned slots);
  uint64_t getGotEntry(Symbol &sym, int64_t addend, GotKind kind);

  // gp is defined to be 0x8000 bytes past the start of its GOT partition, so
  // the partition spans [gp - 32768, gp + 32767].
  static constexpr uint64_t gpBias = 0x8000;
  static constexpr unsigned maxEntriesPerPart = 0x10000 / 8;

  llvm::DenseMap<GotKey, uint64_t> gotEntries;
  llvm::DenseMap<const InputFile *, uint64_t> partOfFile;

  const InputFile *curFile = nullptr;
  // Whether curFile has already been reported as needing more GOT than one gp
  // reaches, in which case its partition is expected to overflow.
  bool curFileOverflows = false;
  uint32_t curPart = 0;
  uint64_t curPartBase = 0;
  unsigned curPartEntries = 0;
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
  case R_ALPHA_GOTDTPREL:
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

// Alpha writes the addend of a dynamic relocation into the place it relocates
// as well as into the relocation itself, so the addend has to be readable back
// out of it -- an assertions build checks that the two agree.
int64_t Alpha::getImplicitAddend(const uint8_t *buf, RelType type) const {
  switch (type) {
  case R_ALPHA_NONE:
  case R_ALPHA_JMP_SLOT:
    return 0;
  case R_ALPHA_REFLONG:
    return SignExtend64<32>(read32(ctx, buf));
  case R_ALPHA_REFQUAD:
  case R_ALPHA_GLOB_DAT:
  case R_ALPHA_RELATIVE:
  case R_ALPHA_TPREL64:
  case R_ALPHA_DTPMOD64:
  case R_ALPHA_DTPREL64:
    return read64(ctx, buf);
  default:
    InternalErr(ctx, buf) << "cannot read addend for relocation " << type;
    return 0;
  }
}

RelType Alpha::getDynRel(RelType type) const {
  if (type == R_ALPHA_REFQUAD)
    return type;
  return R_ALPHA_NONE;
}

uint64_t Alpha::getGp(const InputFile *f) const {
  auto it = partOfFile.find(f);
  uint64_t base = it == partOfFile.end() ? 0 : it->second;
  return ctx.in.got->getVA() + base + gpBias;
}

// The GOT entry a relocation asks for: how many slots it occupies and, in
// `kind`, which of a symbol's entries it is. This mirrors the allocating cases
// of scanSectionImpl, at the larger size wherever scanning has a choice -- a
// TLSGD sequence relaxed to initial exec needs one slot rather than two.
static unsigned gotSlotsFor(RelType type, GotKind &kind) {
  switch (type) {
  case R_ALPHA_LITERAL:
    kind = GK_Addr;
    return 1;
  case R_ALPHA_GOTTPREL:
    kind = GK_TpOff;
    return 1;
  case R_ALPHA_GOTDTPREL:
    kind = GK_DtpOff;
    return 1;
  case R_ALPHA_TLSGD:
    kind = GK_DynTls;
    return 2;
  case R_ALPHA_TLSLDM:
    kind = GK_TlsIndex;
    return 2;
  default:
    return 0;
  }
}

template <class ELFT, class RelTy>
static void addGotDemand(Ctx &ctx, InputSectionBase &sec, Relocs<RelTy> rels,
                         DenseSet<GotKey> &keys, unsigned &slots) {
  for (const RelTy &rel : rels) {
    GotKind kind;
    unsigned n = gotSlotsFor(rel.getType(false), kind);
    if (!n)
      continue;
    // Without an addend in the relocation there is nothing cheap to tell two
    // references to the same symbol apart by, so count them separately.
    if constexpr (!RelTy::HasAddend) {
      slots += n;
      continue;
    } else {
      // TLSLDM ignores both the symbol and the addend: one entry per partition
      // answers for the whole module. getGotEntry keys every other kind on the
      // addend, so distinct addends against one symbol have to be counted
      // separately here too.
      int64_t addend = kind == GK_TlsIndex ? 0 : elf::getAddend<ELFT>(rel);
      Symbol *sym = kind == GK_TlsIndex
                        ? ctx.dummySym
                        : &sec.getFile<ELFT>()->getSymbol(rel.getSymbol(false));
      if (keys.insert({sym, {addend, kind}}).second)
        slots += n;
    }
  }
}

// An upper bound on the GOT entries scanning `f` can allocate: the distinct
// entries its relocations ask for. An entry it shares with a file already in
// the partition is counted a second time here, which only closes a partition
// sooner than it strictly has to.
unsigned Alpha::gotDemand(InputFile *f) const {
  DenseSet<GotKey> keys;
  unsigned slots = 0;
  // The same sections scanRelocations will hand to scanSection.
  for (InputSectionBase *s : cast<ELFFileBase>(f)->getSections()) {
    if (!s || s->kind() != SectionBase::Regular || !s->isLive() ||
        !(s->flags & SHF_ALLOC))
      continue;
    const RelsOrRelas<ELF64LE> rels = s->relsOrRelas<ELF64LE>();
    if (rels.areRelocsCrel())
      addGotDemand<ELF64LE>(ctx, *s, rels.crels, keys, slots);
    else if (rels.areRelocsRel())
      addGotDemand<ELF64LE>(ctx, *s, rels.rels, keys, slots);
    else
      addGotDemand<ELF64LE>(ctx, *s, rels.relas, keys, slots);
  }
  return slots;
}

// Reserve consecutive GOT slots in the current partition and return the byte
// offset of the first one within .got.
uint64_t Alpha::allocGot(unsigned slots) {
  uint64_t off = gotSize;
  for (unsigned i = 0; i != slots; ++i)
    ctx.in.got->reserveEntry();
  gotSize += 8 * slots;
  curPartEntries += slots;
  // The partition was chosen with room for everything gotDemand said this file
  // could ask for, so short of a file too large for any partition -- already
  // reported by now -- overflowing it means the two have drifted apart. Say so
  // rather than emit a binary whose displacements silently do not reach.
  if (curPartEntries > maxEntriesPerPart && !curFileOverflows &&
      curPartEntries - slots <= maxEntriesPerPart)
    InternalErr(ctx, nullptr)
        << "GOT demand of " << curFile << " was undercounted";
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
  GotKey key{&sym, {addend, curPart * GK_Max + kind}};
  auto [it, inserted] = gotEntries.try_emplace(key, 0);
  if (!inserted)
    return it->second;

  GotSection &got = *ctx.in.got;
  uint64_t off = allocGot(kind == GK_DynTls || kind == GK_TlsIndex ? 2 : 1);
  it->second = off;
  bool localInExe = !sym.isPreemptible && !ctx.arg.shared;

  switch (kind) {
  case GK_Addr:
    if (sym.isPreemptible) {
      // No dynamic relocation can express symbol+addend, which is why bfd only
      // ever forms such an entry for a symbol it can resolve itself.  The
      // scanner rejects that case before getting here, where the relocation it
      // came from is still known and can be named.
      assert(!addend && "preemptible literal with an addend reached the GOT");
      ctx.in.relaDyn->addSymbolReloc(gotRel, got, off, sym);
    } else if (ctx.arg.isPic) {
      ctx.in.relaDyn->addRelativeReloc(relativeRel, got, off, sym, addend,
                                       symbolicRel, R_ABS);
    } else {
      got.addConstant({R_ABS, symbolicRel, off, addend, &sym});
    }
    break;
  case GK_DtpOff:
    // Local dynamic can reach an offset too large for R_ALPHA_DTPREL16 through
    // the GOT instead. The dtp base is the TLS segment's address, which is
    // what R_DTPREL computes.
    if (sym.isPreemptible)
      ctx.in.relaDyn->addSymbolReloc(tlsOffsetRel, got, off, sym, addend);
    else
      got.addConstant({R_DTPREL, symbolicRel, off, addend, &sym});
    break;
  case GK_TpOff:
    if (localInExe)
      got.addConstant({R_TPREL, symbolicRel, off, addend, &sym});
    else if (sym.isPreemptible)
      ctx.in.relaDyn->addSymbolReloc(tlsGotRel, got, off, sym, addend);
    else
      // addAddendOnlyRelocIfNonPreemptible, but carrying the addend: the
      // entry is keyed on it, so sym+N needs N in the slot.
      ctx.in.relaDyn->addReloc(/*isAgainstSymbol=*/false, tlsGotRel, got, off,
                               sym, addend, R_ABS, symbolicRel);
    break;
  case GK_DynTls:
    // The module index, then the offset of the symbol within that module's TLS
    // block. In an executable the module index is always 1.
    if (localInExe)
      got.addConstant({R_ADDEND, symbolicRel, off, 1, &sym});
    else
      ctx.in.relaDyn->addSymbolReloc(tlsModuleIndexRel, got, off, sym);
    if (sym.isPreemptible)
      ctx.in.relaDyn->addSymbolReloc(tlsOffsetRel, got, off + 8, sym, addend);
    else
      got.addConstant({R_ABS, tlsOffsetRel, off + 8, addend, &sym});
    break;
  case GK_TlsIndex:
    // Only the module index matters; the second slot stays zero and the caller
    // adds the symbol's dtp offset with R_ALPHA_DTPREL*.
    if (ctx.arg.shared)
      ctx.in.relaDyn->addReloc({tlsModuleIndexRel, &got, off});
    else
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

  // All of a file's code shares one gp, so a partition can only be closed at a
  // file boundary. Close it whenever what is left of it cannot hold everything
  // the file about to be scanned might ask for.
  if (sec.file != curFile) {
    curFile = sec.file;
    unsigned need = gotDemand(sec.file);
    // No arrangement of partitions can help a file that needs more than one gp
    // reaches; give it an empty partition anyway and keep going.
    curFileOverflows = need > maxEntriesPerPart;
    if (curFileOverflows) {
      Err(ctx) << "input file " << curFile
               << " needs more than 64KB of GOT; split it into smaller objects";
      need = maxEntriesPerPart;
    }
    if (curPartEntries + need > maxEntriesPerPart) {
      ++curPart;
      curPartBase = gotSize;
      curPartEntries = 0;
    }
    partOfFile[curFile] = curPartBase;
  }

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
      // A GOT entry the dynamic linker fills in holds the symbol's address and
      // nothing else; no dynamic relocation can express symbol+addend.  bfd
      // only ever forms such an entry for a symbol it resolves itself.
      if (addend && sym.isPreemptible) {
        Err(ctx) << getErrorLoc(ctx, sec.content().data() + offset)
                 << "R_ALPHA_LITERAL against preemptible symbol '" << &sym
                 << "' with a non-zero addend";
        continue;
      }
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_Addr)), &sym});
      continue;
    case R_ALPHA_GOTTPREL:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_TpOff)), &sym});
      continue;
    case R_ALPHA_GOTDTPREL:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_DtpOff)), &sym});
      continue;
    case R_ALPHA_TLSGD:
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_DynTls)), &sym});
      continue;
    case R_ALPHA_TLSLDM:
      // The symbol of a TLSLDM relocation is ignored: the result is always the
      // current module, so one entry per partition suffices.
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
  // The GOT entry's displacement from the gp of the file being relocated.
  case R_ALPHA_LITERAL:
  case R_ALPHA_TLSGD:
  case R_ALPHA_TLSLDM:
  case R_ALPHA_GOTTPREL:
  case R_ALPHA_GOTDTPREL:
    checkInt(ctx, loc, val, 16, rel);
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
