//===-- AlphaAsmBackend.cpp - Alpha assembler backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFixupKinds.h"
#include "AlphaMCTargetDesc.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
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
  case Alpha::fixup_alpha_disp16:
    if (!isInt<16>(int64_t(Value)))
      Ctx.reportError(Loc, "displacement out of range");
    return Value & 0xffff;
  case Alpha::fixup_alpha_lit8:
    // The operate-format literal sits in bits 20-13.
    if (!isUInt<8>(Value))
      Ctx.reportError(Loc, "literal out of range");
    return (Value & 0xff) << 13;
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

  // Allow `.reloc offset, R_ALPHA_FOO, expr` to request a relocation type that
  // no instruction operand can produce, such as the R_ALPHA_LITUSE annotations
  // a linker uses to relax a call.
  std::optional<MCFixupKind> getFixupKind(StringRef Name) const override {
    unsigned Type = llvm::StringSwitch<unsigned>(Name)
#define ELF_RELOC(NAME, ID) .Case(#NAME, ID)
#include "llvm/BinaryFormat/ELFRelocs/Alpha.def"
#undef ELF_RELOC
                        .Default(-1u);
    if (Type == -1u)
      return std::nullopt;
    return static_cast<MCFixupKind>(FirstLiteralRelocationKind + Type);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    // {name, offset, bits, flags}
    const static MCFixupKindInfo Infos[Alpha::NumTargetFixupKinds] = {
        {"fixup_alpha_braddr", 0, 21, 0},
        {"fixup_alpha_literal", 0, 16, 0},
        {"fixup_alpha_gprelhigh", 0, 16, 0},
        {"fixup_alpha_gprellow", 0, 16, 0},
        {"fixup_alpha_gprel16", 0, 16, 0},
        {"fixup_alpha_gpdisp", 0, 16, 0},
        {"fixup_alpha_tprelhi", 0, 16, 0},
        {"fixup_alpha_tprello", 0, 16, 0},
        {"fixup_alpha_gottprel", 0, 16, 0},
        {"fixup_alpha_gotdtprel", 0, 16, 0},
        {"fixup_alpha_tlsgd", 0, 16, 0},
        {"fixup_alpha_tlsldm", 0, 16, 0},
        {"fixup_alpha_dtprelhi", 0, 16, 0},
        {"fixup_alpha_dtprello", 0, 16, 0},
        {"fixup_alpha_hint", 0, 14, 0},
        {"fixup_alpha_lituse_jsr", 0, 0, 0},
        {"fixup_alpha_brsgp", 0, 21, 0},
        {"fixup_alpha_gprel32", 0, 32, 0},
        {"fixup_alpha_disp16", 0, 16, 0},
        {"fixup_alpha_lit8", 13, 8, 0},
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
                       Kind == MCFixupKind(Alpha::fixup_alpha_gotdtprel) ||
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
    Value = adjustFixupValue(Kind, Value, getContext(), Fixup.getLoc());
    // Alpha is little-endian; OR the value into the fixup field.
    for (unsigned I = 0, E = getFixupKindNumBytes(Kind); I != E; ++I)
      Data[I] |= uint8_t((Value >> (I * 8)) & 0xff);
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    OS.write_zeros(Count % 4);
    // GNU as's alpha_handle_align fills a code-alignment gap with one unop
    // (`ldq_u $31, 0($30)', 0x2ffe0000) if the number of words is odd, then
    // repeats an eight-byte nop/unop pair.  The 21064 issues that pair in one
    // cycle, which a run of unops does not, so the pattern is not cosmetic --
    // and a uniform run of unops disagrees with GNU as for every odd-word gap.
    uint64_t Words = Count / 4;
    if (Words & 1) {
      support::endian::write<uint32_t>(OS, 0x2ffe0000,
                                       llvm::endianness::little);
      --Words;
    }
    for (uint64_t I = 0; I != Words; I += 2) {
      support::endian::write<uint32_t>(OS, 0x47ff041f, llvm::endianness::little);
      support::endian::write<uint32_t>(OS, 0x2ffe0000, llvm::endianness::little);
    }
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
