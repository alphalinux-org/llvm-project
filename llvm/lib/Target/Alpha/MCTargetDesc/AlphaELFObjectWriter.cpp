//===-- AlphaELFObjectWriter.cpp - Alpha ELF writer ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFixupKinds.h"
#include "AlphaMCTargetDesc.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class AlphaELFObjectWriter : public MCELFObjectTargetWriter {
public:
  AlphaELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/true, OSABI, ELF::EM_ALPHA,
                                /*HasRelocationAddend=*/true) {}
  ~AlphaELFObjectWriter() override = default;

  // GNU as writes an R_ALPHA_LITUSE immediately after the R_ALPHA_LITERAL it
  // names, pairing them by the sequence number written on both
  // (alpha_adjust_relocs, gas/config/tc-alpha.c), and bfd depends on the
  // adjacency: elf64_alpha_relax_section summarizes how a literal is used by
  // walking the relocations that follow it until the first non-LITUSE, and
  // relaxes `ldq $27,f!literal / jsr ($27)!lituse_jsr' into `bsr f' when it
  // finds a JSR use.  Relocations are otherwise emitted in offset order, so a
  // second literal load scheduled between the first and the call -- which
  // gcc's scheduler does routinely at -O2 -- puts the LITUSE after the wrong
  // LITERAL, and bfd relaxes the call to the wrong function.
  //
  // The assembler tags both halves of a pair with a placeholder relocation
  // holding the sequence number (fixup_alpha_seqmark).  Move each use next to
  // its literal and drop the placeholders.  Codegen emits its pairs adjacent
  // already and tags nothing, so this leaves them alone.
  void sortRelocs(std::vector<ELFRelocationEntry> &Relocs) override {
    // The tag's addend is the sequence number, with the high bit set on the
    // literal's side.  Nothing to do unless the assembler wrote some.
    auto IsMark = [](const ELFRelocationEntry &R) {
      return R.Type == ELF::R_ALPHA_NONE && R.Addend != 0 &&
             (R.Addend & ~uint64_t(0xff)) == 0;
    };
    if (none_of(Relocs, IsMark))
      return;

    // Where each literal's LITERAL entry sits, by sequence number.
    DenseMap<unsigned, uint64_t> LiteralOffset;
    // The sequence number a use at a given offset belongs to.
    DenseMap<uint64_t, unsigned> UseSeq;
    for (const ELFRelocationEntry &R : Relocs) {
      if (!IsMark(R))
        continue;
      if (R.Addend & 0x80)
        LiteralOffset[R.Addend & 0x7f] = R.Offset;
      else
        UseSeq[R.Offset] = R.Addend;
    }

    // The uses to move, keyed by the offset of the literal they belong to.
    // Collecting them first is what lets a use be moved backwards: it is
    // written where its literal is, whichever of the two came first here.
    DenseMap<uint64_t, SmallVector<ELFRelocationEntry, 2>> Moved;
    DenseSet<uint64_t> MovedFrom;
    for (const ELFRelocationEntry &R : Relocs) {
      if (R.Type != ELF::R_ALPHA_LITUSE)
        continue;
      auto Use = UseSeq.find(R.Offset);
      if (Use == UseSeq.end())
        continue;
      auto Lit = LiteralOffset.find(Use->second);
      // A use whose literal produced no relocation -- it was resolved rather
      // than relocated -- keeps its place; there is nothing to sit behind.
      if (Lit == LiteralOffset.end())
        continue;
      Moved[Lit->second].push_back(R);
      MovedFrom.insert(R.Offset);
    }

    std::vector<ELFRelocationEntry> Out;
    Out.reserve(Relocs.size());
    for (const ELFRelocationEntry &R : Relocs) {
      // The markers themselves are the assembler's bookkeeping and are dropped
      // here, so no R_ALPHA_NONE reaches the file.
      if (IsMark(R))
        continue;
      if (R.Type == ELF::R_ALPHA_LITUSE && MovedFrom.contains(R.Offset))
        continue;
      Out.push_back(R);
      if (R.Type == ELF::R_ALPHA_LITERAL) {
        auto It = Moved.find(R.Offset);
        if (It != Moved.end()) {
          Out.insert(Out.end(), It->second.begin(), It->second.end());
          Moved.erase(It);
        }
      }
    }
    // Anything still here named an offset that carried no LITERAL relocation.
    for (auto &KV : Moved)
      Out.insert(Out.end(), KV.second.begin(), KV.second.end());
    Relocs = std::move(Out);
  }

protected:
  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override {
    // A symbol referenced by a TLS relocation must be typed STT_TLS so the
    // linker matches it against the thread-local definition.
    switch (unsigned(Fixup.getKind())) {
    case Alpha::fixup_alpha_tprelhi:
    case Alpha::fixup_alpha_tprello:
    case Alpha::fixup_alpha_gottprel:
    case Alpha::fixup_alpha_tlsgd:
    case Alpha::fixup_alpha_tlsldm:
    case Alpha::fixup_alpha_dtprelhi:
    case Alpha::fixup_alpha_dtprello:
      if (auto *SA = const_cast<MCSymbol *>(Target.getAddSym()))
        static_cast<MCSymbolELF *>(SA)->setType(ELF::STT_TLS);
      break;
    default:
      break;
    }
    switch (unsigned(Fixup.getKind())) {
    case FK_Data_4:
      if (Target.getSpecifier() == Alpha::fixup_alpha_gprel32)
        return ELF::R_ALPHA_GPREL32;
      return IsPCRel ? ELF::R_ALPHA_SREL32 : ELF::R_ALPHA_REFLONG;
    case FK_Data_8:
      return IsPCRel ? ELF::R_ALPHA_SREL64 : ELF::R_ALPHA_REFQUAD;
    case Alpha::fixup_alpha_braddr:
      return ELF::R_ALPHA_BRADDR;
    case Alpha::fixup_alpha_brsgp:
      return ELF::R_ALPHA_BRSGP;
    case Alpha::fixup_alpha_gprel32:
      return ELF::R_ALPHA_GPREL32;
    case Alpha::fixup_alpha_literal:
      return ELF::R_ALPHA_LITERAL;
    case Alpha::fixup_alpha_gprelhigh:
      return ELF::R_ALPHA_GPRELHIGH;
    case Alpha::fixup_alpha_gprellow:
      return ELF::R_ALPHA_GPRELLOW;
    case Alpha::fixup_alpha_gpdisp:
      return ELF::R_ALPHA_GPDISP;
    case Alpha::fixup_alpha_tprelhi:
      return ELF::R_ALPHA_TPRELHI;
    case Alpha::fixup_alpha_tprello:
      return ELF::R_ALPHA_TPRELLO;
    case Alpha::fixup_alpha_gottprel:
      return ELF::R_ALPHA_GOTTPREL;
    case Alpha::fixup_alpha_tlsgd:
      return ELF::R_ALPHA_TLSGD;
    case Alpha::fixup_alpha_tlsldm:
      return ELF::R_ALPHA_TLSLDM;
    case Alpha::fixup_alpha_dtprelhi:
      return ELF::R_ALPHA_DTPRELHI;
    case Alpha::fixup_alpha_dtprello:
      return ELF::R_ALPHA_DTPRELLO;
    case Alpha::fixup_alpha_gprel16:
      return ELF::R_ALPHA_GPREL16;
    case Alpha::fixup_alpha_hint:
      return ELF::R_ALPHA_HINT;
    case Alpha::fixup_alpha_lituse_jsr:
      return ELF::R_ALPHA_LITUSE;
    case Alpha::fixup_alpha_seqmark:
      // sortRelocs drops these; the type only has to be one that carries a
      // plain addend and no symbol.
      return ELF::R_ALPHA_NONE;
    case Alpha::fixup_alpha_disp16:
    case Alpha::fixup_alpha_lit8:
      // No relocation can fill a bare displacement or operate literal, so the
      // expression had to fold to a constant at assembly time.
      reportError(Fixup.getLoc(),
                  "expression is not an assembly-time constant");
      return ELF::R_ALPHA_NONE;
    default:
      reportError(Fixup.getLoc(), "unsupported relocation type");
      return ELF::R_ALPHA_NONE;
    }
  }

  bool needsRelocateWithSymbol(const MCValue &, unsigned Type) const override {
    // The TLS relocations reference the symbol itself.  The GOT literal is left
    // section-relative for a local symbol so the linker can pair it with a
    // lituse_jsr and relax the call; an external symbol keeps its name because
    // it has no local section to fold into.  A GP-relative relocation computes
    // sym + addend - GP, which a section symbol and an addend give just as
    // well, so those do not pin the symbol: pinning them dragged every jump
    // table's block labels and every constant-pool entry into the symbol table,
    // where a symbolizer picked one of them over the function containing it.
    switch (Type) {
    case ELF::R_ALPHA_TPRELHI:
    case ELF::R_ALPHA_TPRELLO:
    case ELF::R_ALPHA_GOTTPREL:
    case ELF::R_ALPHA_TLSGD:
    case ELF::R_ALPHA_TLSLDM:
    case ELF::R_ALPHA_DTPRELHI:
    case ELF::R_ALPHA_DTPRELLO:
    case ELF::R_ALPHA_HINT:
    // The linker checks st_other of the target symbol to apply STO_ALPHA_NOPV
    // / STO_ALPHA_STD_GPLOAD semantics for !samegp optimization.  Using a
    // section-relative reloc would lose the symbol identity and make the
    // linker emit "!samegp reloc against symbol without .prologue".
    case ELF::R_ALPHA_BRSGP:
      return true;
    default:
      return false;
    }
  }
};
} // end anonymous namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createAlphaELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<AlphaELFObjectWriter>(OSABI);
}
