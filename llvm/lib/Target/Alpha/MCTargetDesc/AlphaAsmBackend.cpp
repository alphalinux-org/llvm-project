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
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/EndianStream.h"

using namespace llvm;

static uint64_t adjustFixupValue(unsigned Kind, uint64_t Value) {
  switch (Kind) {
  default:
    return Value;
  case Alpha::fixup_alpha_braddr:
  case Alpha::fixup_alpha_brsgp:
    // 21-bit displacement in instruction units, relative to the instruction
    // after the branch: disp = (target - (branch + 4)) / 4.
    return ((Value - 4) >> 2) & 0x1fffff;
  case Alpha::fixup_alpha_literal:
  case Alpha::fixup_alpha_gprel16:
  case Alpha::fixup_alpha_gprellow:
  case Alpha::fixup_alpha_tprello:
  case Alpha::fixup_alpha_gottprel:
  case Alpha::fixup_alpha_tlsgd:
  case Alpha::fixup_alpha_tlsldm:
  case Alpha::fixup_alpha_dtprello:
    return Value & 0xffff;
  case Alpha::fixup_alpha_gprelhigh:
  case Alpha::fixup_alpha_tprelhi:
  case Alpha::fixup_alpha_dtprelhi:
    return ((Value + 0x8000) >> 16) & 0xffff;
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
        {"fixup_alpha_braddr", 0, 21, 0},    {"fixup_alpha_literal", 0, 16, 0},
        {"fixup_alpha_gprelhigh", 0, 16, 0}, {"fixup_alpha_gprellow", 0, 16, 0},
        {"fixup_alpha_gprel16", 0, 16, 0},   {"fixup_alpha_gpdisp", 0, 16, 0},
        {"fixup_alpha_tprelhi", 0, 16, 0},   {"fixup_alpha_tprello", 0, 16, 0},
        {"fixup_alpha_gottprel", 0, 16, 0},  {"fixup_alpha_tlsgd", 0, 16, 0},
        {"fixup_alpha_tlsldm", 0, 16, 0},    {"fixup_alpha_dtprelhi", 0, 16, 0},
        {"fixup_alpha_dtprello", 0, 16, 0},  {"fixup_alpha_hint", 0, 14, 0},
        {"fixup_alpha_lituse_jsr", 0, 0, 0}, {"fixup_alpha_brsgp", 0, 21, 0},
        {"fixup_alpha_gprel32", 0, 32, 0},
    };
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
    MCFixupKind Kind = Fixup.getKind();
    // The GOT/GP-relative and gpdisp displacements are always filled by a
    // relocation, even when the fixup value is locally known.
    bool AlwaysReloc = Kind == MCFixupKind(Alpha::fixup_alpha_literal) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_gprelhigh) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_gprellow) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_gprel16) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_gpdisp) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_tprelhi) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_tprello) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_gottprel) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_tlsgd) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_tlsldm) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_dtprelhi) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_dtprello) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_hint) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_lituse_jsr) ||
                       Kind == MCFixupKind(Alpha::fixup_alpha_brsgp);
    maybeAddReloc(F, Fixup, Target, Value, AlwaysReloc ? false : IsResolved);
    if (mc::isRelocation(Kind) || AlwaysReloc)
      return;
    if (!IsResolved)
      return;
    Value = adjustFixupValue(Kind, Value);
    if (!Value)
      return;
    // All Alpha instructions are four little-endian bytes; OR the value in.
    for (unsigned I = 0; I != 4; ++I)
      Data[I] |= uint8_t((Value >> (I * 8)) & 0xff);
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    OS.write_zeros(Count % 4);
    // Pad with unop (`ldq_u $31, 0($30)`, 0x2ffe0000), the canonical Alpha
    // code-alignment filler that GNU as also uses.
    for (uint64_t I = 0, E = Count / 4; I != E; ++I)
      support::endian::write<uint32_t>(OS, 0x2ffe0000,
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
