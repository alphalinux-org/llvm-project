//===-- AlphaAsmBackend.cpp - Alpha assembler backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFixupKinds.h"
#include "AlphaMCTargetDesc.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

static uint64_t adjustFixupValue(unsigned Kind, uint64_t Value, MCContext &Ctx,
                                 SMLoc Loc) {
  switch (Kind) {
  default:
    return Value;
  case Alpha::fixup_alpha_braddr:
    // 21-bit displacement in instruction units, relative to the instruction
    // after the branch: disp = (target - (branch + 4)) / 4.
    if ((Value - 4) & 3)
      Ctx.reportError(Loc, "branch target must be 4-byte aligned");
    else if (!isInt<23>(int64_t(Value) - 4))
      Ctx.reportError(Loc, "branch target out of range");
    return ((Value - 4) >> 2) & 0x1fffff;
  }
}

// Instruction fixups all sit in a four-byte instruction word; the FK_Data_*
// kinds MC hands this backend for a resolved .byte/.short/.quad difference
// carry their own width.
static unsigned getFixupKindNumBytes(unsigned Kind) {
  switch (Kind) {
  case FK_Data_1:
    return 1;
  case FK_Data_2:
    return 2;
  case FK_Data_8:
    return 8;
  default:
    return 4;
  }
}

namespace {
class AlphaAsmBackend : public MCAsmBackend {
  uint8_t OSABI;

public:
  AlphaAsmBackend(uint8_t OSABI)
      : MCAsmBackend(llvm::endianness::little), OSABI(OSABI) {}

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    // {name, offset, bits, flags}
    const static MCFixupKindInfo Infos[Alpha::NumTargetFixupKinds] = {
        {"fixup_alpha_braddr", 0, 21, 0},
        {"fixup_alpha_literal", 0, 16, 0},
        {"fixup_alpha_gprelhigh", 0, 16, 0},
        {"fixup_alpha_gprellow", 0, 16, 0},
    };
    // Infos is indexed positionally by Kind - FirstTargetFixupKind, so its
    // rows stay in the order of enum Fixups in AlphaFixupKinds.h.
    static_assert(std::size(Infos) == Alpha::NumTargetFixupKinds,
                  "a fixup kind has no info row");
    if (mc::isRelocation(Kind))
      return {};
    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);
    assert(unsigned(Kind - FirstTargetFixupKind) < Alpha::NumTargetFixupKinds &&
           "Invalid kind!");
    return Infos[Kind - FirstTargetFixupKind];
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    maybeAddReloc(F, Fixup, Target, Value, IsResolved);
    MCFixupKind Kind = Fixup.getKind();
    if (mc::isRelocation(Kind))
      return;
    // GOT- and GP-relative displacements are always resolved by a relocation.
    if (Kind == MCFixupKind(Alpha::fixup_alpha_literal) ||
        Kind == MCFixupKind(Alpha::fixup_alpha_gprelhigh) ||
        Kind == MCFixupKind(Alpha::fixup_alpha_gprellow))
      return;
    if (!IsResolved)
      return;
    Value = adjustFixupValue(Kind, Value, getContext(), Fixup.getLoc());
    // Alpha is little-endian; OR the value into the fixup field.
    for (unsigned I = 0, E = getFixupKindNumBytes(Kind); I != E; ++I)
      Data[I] |= uint8_t((Value >> (I * 8)) & 0xff);
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    OS.write_zeros(Count % 4);
    // nop is `bis $31, $31, $31` (0x47ff041f).
    for (uint64_t I = 0, E = Count / 4; I != E; ++I)
      support::endian::write<uint32_t>(OS, 0x47ff041f,
                                       llvm::endianness::little);
    return true;
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createAlphaELFObjectWriter(OSABI);
  }
};
} // end anonymous namespace

MCAsmBackend *llvm::createAlphaAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  uint8_t OSABI =
      MCELFObjectTargetWriter::getOSABI(STI.getTargetTriple().getOS());
  return new AlphaAsmBackend(OSABI);
}
