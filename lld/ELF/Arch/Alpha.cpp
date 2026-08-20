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
// --relax undoes some of that indirection: a call whose target the linker can
// see and reach is rewritten into a direct branch. See relaxOnce.
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

// The addend of an R_ALPHA_LITUSE says how the annotated instruction uses the
// literal it was paired with: a call, or the call in a dynamic TLS sequence.
enum { LITUSE_ALPHA_JSR = 3, LITUSE_ALPHA_TLSGD = 4, LITUSE_ALPHA_TLSLDM = 5 };

// The instruction opcodes --relax has to recognize or produce. jsr and jmp
// share an opcode and are told apart by their function field.
enum {
  OP_LDA = 0x08,
  OP_LDAH = 0x09,
  OP_JSR = 0x1a,
  OP_LDQ = 0x29,
  OP_BR = 0x30,
  OP_BSR = 0x34
};
enum { FUNC_JMP = 0, FUNC_JSR = 1 };

// Whole instructions --relax writes: the canonical no-op, the PALcode call that
// reads the thread pointer into $0, and the add of a thread-pointer offset in
// $16 to it, which together stand in for a call to __tls_get_addr.
constexpr uint32_t INSN_UNOP = 0x2ffe0000;   // ldq_u $31, 0($30)
constexpr uint32_t INSN_RDUNIQ = 0x0000009e; // call_pal rduniq
// $16 here is deliberate and is not the `arg' register the first instruction
// pair is rewritten with. The argument reaches __tls_get_addr in $16 whatever
// register the offset was computed in: if a compiler hoisted the first pair out
// of a loop and used another register, it left a move into $16 ahead of the
// call, and that move is not part of the sequence being rewritten. So by this
// point $16 holds the offset either way. bfd hardcodes 16 here for the same
// reason (elf64_alpha_relax_tls_get_addr).
constexpr uint32_t INSN_ADDQ_TP = 0x42000400; // addq $16, $0, $0

// A memory-format instruction with a zero displacement, which a relocation at
// the same offset fills in.
static uint32_t memInsn(unsigned op, unsigned ra, unsigned rb) {
  return (op << 26) | (ra << 21) | (rb << 16);
}

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

// An ifunc we resolve ourselves with R_ALPHA_IRELATIVE. A preemptible one is
// left to the dynamic linker, which resolves it like any other symbol.
static bool isAlphaIfunc(Ctx &ctx, const Symbol &sym) {
  return sym.isGnuIFunc() && !sym.isPreemptible;
}

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
  bool relaxOnce(int pass) const override;

  uint64_t getGp(const InputFile *f) const override;

private:
  unsigned gotDemand(InputFile *f) const;
  uint64_t allocGot(unsigned slots);
  uint64_t getGotEntry(Symbol &sym, int64_t addend, GotKind kind);

  // A GOT literal load together with the calls that consume it, recorded while
  // scanning so that --relax can consider them once addresses are known.
  struct RelaxCall {
    InputSection *sec;
    Symbol *sym;
    int64_t addend;
    uint64_t litOffset;
    // Offsets of the jsr instructions annotated with LITUSE_ALPHA_JSR.
    SmallVector<uint64_t, 1> jsrOffsets;
    // Whether those calls are the only uses of the literal.
    bool onlyJsrUses;
  };
  SmallVector<RelaxCall, 0> relaxCalls;

  // For each GOT entry a literal load reads: how many loads read it, and
  // whether it is a plain constant. An entry every one of whose loads has been
  // deleted can be given back, but only a constant one: dropping a dynamic
  // relocation as well would resize .rela.dyn underneath a layout that has
  // already been fixed.
  llvm::DenseMap<uint64_t, unsigned> litUses;
  llvm::DenseSet<uint64_t> litDynamic;
  bool reclaimGot(const llvm::DenseMap<uint64_t, unsigned> &dropped) const;

  // gp is defined to be 0x8000 bytes past the start of its GOT partition, so
  // the partition spans [gp - 32768, gp + 32767].
  static constexpr uint64_t gpBias = 0x8000;
  static constexpr unsigned maxEntriesPerPart = 0x10000 / 8;

  llvm::DenseMap<GotKey, uint64_t> gotEntries;
  mutable llvm::DenseMap<const InputFile *, uint64_t> partOfFile;

  const InputFile *curFile = nullptr;
  // Whether curFile has already been reported as needing more GOT than one gp
  // reaches, in which case its partition is expected to overflow.
  bool curFileOverflows = false;
  uint32_t curPart = 0;
  uint64_t curPartBase = 0;
  unsigned curPartEntries = 0;
  mutable uint64_t gotSize = 0;
};
} // namespace

Alpha::Alpha(Ctx &ctx) : TargetInfo(ctx) {
  copyRel = R_ALPHA_COPY;
  gotRel = R_ALPHA_GLOB_DAT;
  pltRel = R_ALPHA_JMP_SLOT;
  relativeRel = R_ALPHA_RELATIVE;
  iRelativeRel = R_ALPHA_IRELATIVE;
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
  // R_ALPHA_LITERAL. It never contributes a value; scanSectionImpl collects it
  // for --relax.
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
    // An ifunc has no link-time address to point at either.
    return s.isPreemptible || s.isGnuIFunc() ? R_NONE : R_PC;
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
  case R_ALPHA_IRELATIVE:
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
    // Only an entry holding nothing but a constant can be reclaimed later.
    if (isAlphaIfunc(ctx, sym) || sym.isPreemptible || ctx.arg.isPic)
      litDynamic.insert(off);
    if (isAlphaIfunc(ctx, sym)) {
      // The runtime calls the resolver named by the addend and stores its
      // result here, so every reference through the GOT sees one address.
      getIRelativeSection(ctx).addReloc(/*isAgainstSymbol=*/false, iRelativeRel,
                                        got, off, sym, addend, R_ABS,
                                        R_ALPHA_REFQUAD);
    } else if (sym.isPreemptible) {
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

// Alpha addresses every TLS GOT slot through gp, and its general- and
// local-dynamic sequences call __tls_get_addr through a separate
// R_ALPHA_LITERAL, so a GD or LD relocation cannot be rewritten into IE or LE
// on its own. relaxTlsCall rewrites the whole five-instruction sequence at
// once; without --relax, or where the rewrite does not apply, every model keeps
// its own GOT slots.
template <class ELFT, class RelTy>
void Alpha::scanSectionImpl(InputSectionBase &sec, Relocs<RelTy> rels,
                            unsigned shard) {
  RelocScan rs(ctx, &sec, shard);
  sec.relocations.reserve(rels.size());

  // Only code can be relaxed, and only in a section whose bytes are laid out
  // as-is; relaxCalls indexes into sec.relocations later on.
  auto *isec = dyn_cast<InputSection>(&sec);
  bool canRelax = ctx.arg.relax && isec && (sec.flags & SHF_EXECINSTR);

  // Deleting a GOT load is only safe when every use of it is accounted for, and
  // the only record of those uses is that GNU as pairs each R_ALPHA_LITUSE with
  // the R_ALPHA_LITERAL it follows. A lituse that follows no literal means one
  // literal is being shared between sequences -- llvm's own code generator does
  // this to a TLS sequence's __tls_get_addr load -- and there is then no way to
  // tell which loads are still live, so give up on the whole section. bfd does
  // not check, and miscompiles such objects.
  if (canRelax) {
    bool paired = false;
    for (auto it = rels.begin(); it != rels.end(); ++it) {
      RelType t = it->getType(false);
      if (t == R_ALPHA_LITERAL)
        paired = true;
      else if (t == R_ALPHA_LITUSE) {
        if (!paired) {
          canRelax = false;
          break;
        }
      } else if (t != R_ALPHA_HINT)
        paired = false;
    }
  }
  // The candidate the R_ALPHA_LITUSE relocations being scanned belong to.
  int curLit = -1;
  // Offsets of the relocations a rewritten TLS sequence has already consumed.
  llvm::DenseSet<uint64_t> tlsRelaxed;

  // Rewrite a general- or local-dynamic sequence into initial- or local-exec.
  // gcc and clang both emit it as
  //
  //     lda  $16, x($gp)                !tlsgd
  //     ldq  $27, __tls_get_addr($gp)   !literal
  //     jsr  $26, ($27)                 !lituse_tlsgd
  //     ldah $29, 0($26)                !gpdisp
  //     lda  $29, 0($29)
  //
  // and the call can be replaced by reading the thread pointer directly:
  //
  //     ldah $16, x($31)                !tprelhi     (or ldq $16, x($gp)
  //     !gottprel) lda  $16, x($16)                !tprello     (or unop)
  //     call_pal rduniq
  //     addq $16, $0, $0
  //     unop
  //
  // Like bfd, this only applies in a non-PIC executable: everywhere else the
  // thread-pointer offset is not a link-time constant and the module the symbol
  // belongs to is not necessarily this one. Local exec needs the symbol to be
  // non-preemptible as well; otherwise the offset still comes from the GOT, but
  // through one entry instead of two plus a literal.
  auto relaxTlsCall = [&](auto it, RelType type, Symbol &sym,
                          int64_t addend) -> bool {
    if (!canRelax || ctx.arg.isPic)
      return false;
    bool isGd = type == R_ALPHA_TLSGD;
    // The literal that loads __tls_get_addr and the lituse that marks the call
    // must follow immediately, as GNU as emits them.
    auto lit = it, use = it;
    if (++lit == rels.end() || ++(use = lit) == rels.end())
      return false;
    if (lit->getType(false) != R_ALPHA_LITERAL ||
        use->getType(false) != R_ALPHA_LITUSE ||
        rs.getAddend<ELFT>(*use, R_ALPHA_LITUSE) !=
            (isGd ? LITUSE_ALPHA_TLSGD : LITUSE_ALPHA_TLSLDM))
      return false;
    // The gp reload after the call is what the last two instructions are.
    uint64_t gpdispOff = use->r_offset + 4;
    auto gpdisp = use;
    while (++gpdisp != rels.end() && gpdisp->r_offset <= gpdispOff)
      if (gpdisp->r_offset == gpdispOff &&
          gpdisp->getType(false) == R_ALPHA_GPDISP)
        break;
    if (gpdisp == rels.end() || gpdisp->r_offset != gpdispOff ||
        gpdisp->getType(false) != R_ALPHA_GPDISP)
      return false;

    // Rewriting in place must not reorder the register lifetimes, so the
    // instructions have to appear in the order the relocations do.
    uint64_t p0 = it->r_offset, p1 = lit->r_offset, p2 = use->r_offset,
             p3 = gpdispOff,
             p4 = p3 + rs.getAddend<ELFT>(*gpdisp, R_ALPHA_GPDISP);
    if (!(p0 < p1 && p1 < p2 && p2 < p3) || p4 + 4 > sec.content().size() ||
        p0 + 4 > sec.content().size())
      return false;

    // The register the sequence computes its argument in. A compiler may hoist
    // part of the sequence and use something other than $16, moving it into
    // place before the call, so only the first pair may use this register.
    unsigned arg = (read32le(sec.content().data() + p0) >> 21) & 31;

    // Local exec needs a link-time thread-pointer offset, and needs the two
    // instructions to be adjacent so the offset can be split across them.
    if (!sym.isPreemptible && p0 + 4 == p1) {
      // A local-dynamic sequence resolves to the module's own TLS block, which
      // is this executable's, so its offset is that of the block itself.
      RelExpr expr = isGd ? R_TPREL : RE_ALPHA_TPREL_BASE;
      Symbol *target = isGd ? &sym : ctx.dummySym;
      int64_t off = isGd ? addend : 0;
      sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p0,
                    memInsn(OP_LDAH, arg, 31), &sym});
      sec.addReloc({expr, R_ALPHA_TPRELHI, p0, off, target});
      sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p1,
                    memInsn(OP_LDA, arg, arg), &sym});
      sec.addReloc({expr, R_ALPHA_TPRELLO, p1, off, target});
    } else if (isGd) {
      // Initial exec: one GOT entry holding the thread-pointer offset.
      sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p0,
                    memInsn(OP_LDQ, arg, 29), &sym});
      sec.addReloc({RE_ALPHA_GOT, R_ALPHA_GOTTPREL, p0,
                    int64_t(getGotEntry(sym, addend, GK_TpOff)), &sym});
      sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p1, INSN_UNOP, &sym});
    } else {
      return false;
    }

    sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p2, INSN_RDUNIQ, &sym});
    sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p3, INSN_ADDQ_TP, &sym});
    sec.addReloc({RE_ALPHA_RELAX_INSN, R_ALPHA_NONE, p4, INSN_UNOP, &sym});

    // The literal, the lituse, the gp reload and any hint on the call are all
    // gone with the call itself.
    tlsRelaxed.insert(p1);
    tlsRelaxed.insert(p2);
    tlsRelaxed.insert(p3);
    return true;
  };

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

    // Everything a rewritten TLS sequence covers has already been dealt with.
    if (!tlsRelaxed.empty() && tlsRelaxed.contains(offset))
      continue;

    // An R_ALPHA_LITUSE annotates the instruction that uses the literal loaded
    // for the R_ALPHA_LITERAL it follows, so anything else in between ends the
    // group -- except an R_ALPHA_HINT, which shares the jsr's offset and so
    // sits alongside the LITUSE rather than between it and the literal. Both
    // GNU as and our own emitter write the LITUSE first, which is why no test
    // can reach this arm: by the time the hint is seen the call has already
    // been recorded. It is here so that the order stays a detail of the
    // producer rather than something correctness rests on.
    if (type != R_ALPHA_LITERAL && type != R_ALPHA_LITUSE &&
        type != R_ALPHA_HINT)
      curLit = -1;

    switch (type) {
    case R_ALPHA_REFQUAD:
      if (!isAlphaIfunc(ctx, sym))
        break;
      // A data reference to an ifunc is resolved at startup, so it has to
      // land somewhere writable.
      if (!(sec.flags & SHF_WRITE)) {
        Err(ctx) << getErrorLoc(ctx, sec.content().data() + offset)
                 << "cannot resolve ifunc '" << &sym
                 << "' in read-only section " << sec.name;
        continue;
      }
      getIRelativeSection(ctx).addReloc(/*isAgainstSymbol=*/false,
                                        R_ALPHA_IRELATIVE, sec, offset, sym,
                                        addend, R_ABS, R_ALPHA_REFQUAD);
      continue;
    case R_ALPHA_BRADDR:
    case R_ALPHA_BRSGP:
      // Without a PLT there is no stub to branch to, and an ifunc has no
      // link-time address of its own.
      if (isAlphaIfunc(ctx, sym)) {
        Err(ctx) << getErrorLoc(ctx, sec.content().data() + offset)
                 << "cannot branch directly to ifunc '" << &sym
                 << "'; it must be called through the GOT";
        continue;
      }
      break;
    // These consume a GOT entry. Allocate it now and carry its offset within
    // .got in the addend; RE_ALPHA_GOT turns that into a gp displacement.
    case R_ALPHA_LITERAL: {
      // A GOT entry the dynamic linker fills in holds the symbol's address and
      // nothing else; no dynamic relocation can express symbol+addend.  bfd
      // only ever forms such an entry for a symbol it resolves itself.
      if (addend && sym.isPreemptible && !isAlphaIfunc(ctx, sym)) {
        Err(ctx) << getErrorLoc(ctx, sec.content().data() + offset)
                 << "R_ALPHA_LITERAL against preemptible symbol '" << &sym
                 << "' with a non-zero addend";
        continue;
      }
      uint64_t gotOff = getGotEntry(sym, addend, GK_Addr);
      ++litUses[gotOff];
      sec.addReloc({RE_ALPHA_GOT, type, offset, int64_t(gotOff), &sym});
    }
      // Remember the load in case a call turns out to consume it. A preemptible
      // symbol has no link-time address to branch to, and an ifunc's address is
      // whatever its resolver returns, so neither can ever be relaxed.
      curLit = -1;
      if (canRelax && isa<Defined>(sym) && !sym.isPreemptible &&
          !sym.isGnuIFunc()) {
        curLit = relaxCalls.size();
        relaxCalls.push_back({isec, &sym, addend, offset, {}, true});
      }
      continue;
    case R_ALPHA_LITUSE:
      // Any use other than a call means the loaded address is still needed.
      if (curLit >= 0) {
        if (addend == LITUSE_ALPHA_JSR)
          relaxCalls[curLit].jsrOffsets.push_back(offset);
        else
          relaxCalls[curLit].onlyJsrUses = false;
      }
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
      if (relaxTlsCall(it, type, sym, addend))
        continue;
      sec.addReloc({RE_ALPHA_GOT, type, offset,
                    int64_t(getGotEntry(sym, addend, GK_DynTls)), &sym});
      continue;
    case R_ALPHA_TLSLDM:
      if (relaxTlsCall(it, type, sym, addend))
        continue;
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

// --relax turns a call made through the GOT,
//
//     ldq $27, callee($gp)   !literal
//     jsr $26, ($27)         !lituse_jsr
//
// into a direct `bsr $26, callee`, which needs no GOT load at run time and is
// predicted rather than speculated. It applies whenever the callee is close
// enough for the 21-bit displacement to reach it. A tail call, which is the
// same sequence with a jmp in place of the jsr, becomes a br.
//
// Dropping the load as well is only sound if the callee does not need its own
// address. st_other says whether it does: a function marked NOPV never reads
// it, and one marked with the standard two-instruction gp load reads it only to
// compute a gp, so the call can enter it eight bytes in -- provided that is the
// gp the caller already has, which with multi-GOT is not a given. Otherwise the
// load stays and only the branch is rewritten, which remains correct because
// the callee still derives its gp from $27.
//
// GOT entries are allocated during scanning, long before any address is known,
// so an entry no surviving load reads has to be given back afterwards; see
// reclaimGot. Doing that moves everything laid out after .got, which is why
// this runs as a relaxation pass rather than at the end of one.
bool Alpha::relaxOnce(int pass) const {
  // Nothing here changes a section's size, so one pass settles it.
  if (pass != 0)
    return false;

  InputSection *cur = nullptr;
  // The relocations of `cur` by offset, so that the one on a jsr or on the load
  // can be found without rescanning the section for every call.
  DenseMap<uint64_t, unsigned> relocIndex;
  SmallVector<Relocation, 0> added;

  auto flush = [&] {
    if (added.empty())
      return;
    llvm::append_range(cur->relocations, added);
    llvm::stable_sort(cur->relocations,
                      [](const Relocation &a, const Relocation &b) {
                        return a.offset < b.offset;
                      });
    added.clear();
  };

  // How many of each GOT entry's loads have gone away.
  DenseMap<uint64_t, unsigned> dropped;

  for (const RelaxCall &c : relaxCalls) {
    if (c.sec != cur) {
      flush();
      cur = c.sec;
      relocIndex.clear();
      for (auto [i, r] : llvm::enumerate(cur->relocations))
        relocIndex[r.offset] = i;
    }
    if (!c.sec->isLive() || c.jsrOffsets.empty())
      continue;

    // The literal has to have been loaded by a ldq for the calls to be using
    // the register this says they are.
    ArrayRef<uint8_t> content = c.sec->content();
    if (c.litOffset + 4 > content.size())
      continue;
    uint32_t lit = read32le(content.data() + c.litOffset);
    if ((lit >> 26) != OP_LDQ)
      continue;
    unsigned pvReg = (lit >> 21) & 31;

    // What the callee does with its procedure value, as st_other advertises it.
    // A callee marked NOPV never looks at it, so the load can go and the branch
    // lands on the callee itself. One marked STD_GPLOAD looks at it only to
    // compute a gp, which the call can skip by entering eight bytes in -- but
    // only if that is the gp the caller already has, which across GOT
    // partitions it is not. bfd additionally recognizes an unmarked callee
    // whose first two words carry a GPDISP; we take the marking at face value.
    const Defined &d = cast<Defined>(*c.sym);
    auto *dsec = dyn_cast_or_null<InputSectionBase>(d.section);
    // st_other describes what the symbol's own entry point does with the
    // procedure value.  A literal carrying an addend names somewhere else,
    // where it says nothing -- entering eight bytes past that is not entering
    // past a gp load, so there is nothing to skip.
    unsigned pvUse = c.addend == 0 ? (d.stOther & STO_ALPHA_STD_GPLOAD) : 0;
    bool skipGpLoad = pvUse == STO_ALPHA_STD_GPLOAD && dsec &&
                      getGp(c.sec->file) == getGp(dsec->file);
    bool dropLoad = c.onlyJsrUses && (pvUse == STO_ALPHA_NOPV || skipGpLoad);
    int64_t addend = c.addend + (skipGpLoad ? 8 : 0);
    uint64_t dest = d.getVA(ctx, addend);

    unsigned relaxed = 0;
    for (uint64_t off : c.jsrOffsets) {
      if (off + 4 > content.size())
        continue;
      // A ret or a jsr_coroutine takes its target from the return stack rather
      // than from the literal, and a call through some other register is not
      // this literal's use at all.
      uint32_t insn = read32le(content.data() + off);
      unsigned func = (insn >> 14) & 3;
      if ((insn >> 26) != OP_JSR || (func != FUNC_JSR && func != FUNC_JMP) ||
          ((insn >> 16) & 31) != pvReg)
        continue;
      // Displacements are measured from the instruction after the branch.
      int64_t disp = int64_t(dest) - int64_t(c.sec->getVA(off) + 4);
      if ((disp & 3) || !isInt<23>(disp))
        continue;

      // A call to a symbol outside this object also carries an R_ALPHA_HINT on
      // the jsr. Replace it: its 14-bit field overlaps the displacement the
      // branch now needs, so it must not be applied on top.
      Relocation r{RE_ALPHA_RELAX_JSR, R_ALPHA_BRADDR, off, addend, c.sym};
      if (auto it = relocIndex.find(off); it != relocIndex.end())
        c.sec->relocations[it->second] = r;
      else
        added.push_back(r);
      ++relaxed;
    }

    // The load is dead only once every one of its uses is gone.
    if (dropLoad && relaxed == c.jsrOffsets.size()) {
      Relocation &r = c.sec->relocations[relocIndex.find(c.litOffset)->second];
      ++dropped[uint64_t(r.addend)];
      r.expr = RE_ALPHA_RELAX_INSN;
      r.addend = INSN_UNOP;
    }
  }
  flush();
  return reclaimGot(dropped);
}

// Give back the GOT entries whose every load has just been deleted, and slide
// the rest down over them. The offset of an entry appears in the addend of each
// RE_ALPHA_GOT relocation that reads it, in the relocation that initializes it,
// and in any dynamic relocation against it; a partition's base, which fixes a
// gp, is an offset too. Returns whether anything moved, which makes the caller
// lay the output out again.
bool Alpha::reclaimGot(const DenseMap<uint64_t, unsigned> &dropped) const {
  if (dropped.empty())
    return false;

  // An entry survives unless it is a constant that nothing reads any more. TLS
  // entries occupy two slots and are never dropped, so stepping by one slot
  // never lands in the middle of a live entry.
  unsigned slots = gotSize / 8;
  SmallVector<uint64_t, 0> remap(slots + 1);
  uint64_t kept = 0;
  bool changed = false;
  for (unsigned i = 0; i != slots + 1; ++i) {
    // The new base of a partition starting here is where the next entry lands,
    // so record that before deciding this slot's fate.
    remap[i] = kept * 8;
    if (i == slots)
      break;
    uint64_t off = i * 8;
    auto it = dropped.find(off);
    if (it != dropped.end() && it->second == litUses.lookup(off) &&
        !litDynamic.contains(off))
      changed = true;
    else
      ++kept;
  }
  if (!changed)
    return false;

  auto move = [&](uint64_t off) { return remap[off / 8]; };
  for (InputSectionBase *sec : ctx.inputSections)
    for (Relocation &r : sec->relocations)
      if (r.expr == RE_ALPHA_GOT)
        r.addend = int64_t(move(uint64_t(r.addend)));

  // The relocations that fill the table in are indexed by offset within it, so
  // the ones belonging to a dropped entry go away with it.
  GotSection &got = *ctx.in.got;
  llvm::erase_if(got.relocations, [&](const Relocation &r) {
    return remap[r.offset / 8] == remap[r.offset / 8 + 1];
  });
  for (Relocation &r : got.relocations)
    r.offset = move(r.offset);

  auto moveDyn = [&](SmallVectorImpl<DynamicReloc> &relocs) {
    for (DynamicReloc &r : relocs)
      if (r.inputSec == &got)
        r.offsetInSec = move(r.offsetInSec);
  };
  if (ctx.in.relaDyn) {
    moveDyn(ctx.in.relaDyn->relocs);
    moveDyn(ctx.in.relaDyn->relativeRelocs);
  }
  if (ctx.in.relaPlt)
    moveDyn(ctx.in.relaPlt->relocs);

  for (auto &kv : partOfFile)
    kv.second = move(kv.second);

  gotSize = kept * 8;
  got.dropEntriesAfter(kept);
  return true;
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
  // A relaxed call replaces the instruction rather than patching a field, so it
  // is keyed on the expression; the type stays the one the result needs.
  switch (rel.expr) {
  case RE_ALPHA_RELAX_INSN:
    // A whole replacement instruction. Any ordinary relocation at this offset
    // is applied after it and fills in the immediate.
    write32le(loc, uint32_t(val));
    return;
  case RE_ALPHA_RELAX_JSR: {
    // bsr keeps the return-address register the jsr named in Ra, and pushes the
    // predictor's call stack as the jsr did. A jmp, which discards the return
    // address, becomes the br that does not.
    uint32_t insn = read32le(loc);
    uint32_t op = ((insn >> 14) & 3) == FUNC_JSR ? OP_BSR : OP_BR;
    int64_t disp = int64_t(val) - 4;
    // The call was in range when it was relaxed, but giving GOT entries back
    // moves whatever is laid out after .got, so say so rather than truncate.
    checkInt(ctx, loc, disp, 23, rel);
    write32le(loc, (op << 26) | (insn & 0x03e00000) | ((disp >> 2) & 0x1fffff));
    return;
  }
  default:
    break;
  }

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
