//===-- AlphaELFObjectWriter.cpp - Alpha ELF writer ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFixupKinds.h"
#include "AlphaMCTargetDesc.h"
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
      if (auto *SA = const_cast<MCSymbol *>(Target.getAddSym()))
        static_cast<MCSymbolELF *>(SA)->setType(ELF::STT_TLS);
      break;
    default:
      break;
    }
    switch (unsigned(Fixup.getKind())) {
    case FK_Data_4:
      return IsPCRel ? ELF::R_ALPHA_SREL32 : ELF::R_ALPHA_REFLONG;
    case FK_Data_8:
      return IsPCRel ? ELF::R_ALPHA_SREL64 : ELF::R_ALPHA_REFQUAD;
    case Alpha::fixup_alpha_braddr:
      return ELF::R_ALPHA_BRADDR;
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
    default:
      reportError(Fixup.getLoc(), "unsupported relocation type");
      return ELF::R_ALPHA_NONE;
    }
  }

  bool needsRelocateWithSymbol(const MCValue &, unsigned Type) const override {
    // The GOT/GP-relative and TLS relocations reference the symbol itself.
    switch (Type) {
    case ELF::R_ALPHA_LITERAL:
    case ELF::R_ALPHA_GPRELHIGH:
    case ELF::R_ALPHA_GPRELLOW:
    case ELF::R_ALPHA_TPRELHI:
    case ELF::R_ALPHA_TPRELLO:
    case ELF::R_ALPHA_GOTTPREL:
    case ELF::R_ALPHA_TLSGD:
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
