//===-- AlphaMCTargetDesc.h - Alpha Target Descriptions ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides Alpha specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCTARGETDESC_H
#define LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCTARGETDESC_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {

namespace Alpha {
// The floating-point rounding modes (-mfp-rounding-mode); Normal is the
// default.
enum FPRoundMode {
  FPRoundNormal,
  FPRoundChopped,
  FPRoundMinus,
  FPRoundDynamic
};

// The floating-point trap-qualifier letters (without the leading '/') for an
// instruction's trap class under -mieee / -mieee-with-inexact / -mfp-trap-mode.
// The class values match TrapClass in AlphaInstrFormats.td.  IEEE selects the
// software-completion (/s) modes; TrapU is the bare -mfp-trap-mode=u.
inline StringRef getFPTrapSuffix(unsigned TrapClass, bool IEEE, bool Inexact,
                                 bool TrapU) {
  switch (TrapClass) {
  case 1: // Arithmetic: underflow.
    return IEEE ? (Inexact ? "sui" : "su") : (TrapU ? "u" : StringRef());
  case 2: // Compare: software completion only.
    return IEEE ? "su" : StringRef();
  case 3: // Float-to-integer: integer overflow.
    return IEEE ? (Inexact ? "svi" : "sv") : (TrapU ? "v" : StringRef());
  case 4: // Integer-to-float: inexact only.
    return (IEEE && Inexact) ? "sui" : StringRef();
  case 5:
    return IEEE ? "s"
                : StringRef(); // S-to-T convert (software completion only).
  default:
    return StringRef();
  }
}

// The amount added to the instruction's function field for the trap qualifier:
// 0x500 for su/sv, 0x700 for sui/svi, 0x100 for a bare u/v.
inline unsigned getFPTrapFuncBits(StringRef Suffix) {
  if (Suffix.empty())
    return 0;
  if (Suffix == "s")
    return 0x400;
  if (Suffix == "sui" || Suffix == "svi")
    return 0x700;
  if (Suffix == "su" || Suffix == "sv")
    return 0x500;
  return 0x100; // u / v
}

// The ambient rounding mode -- what -mfp-rounding-mode asks for -- applies to
// the arithmetic (1) and integer-to-float (4) operate instructions.  It must
// not reach float-to-integer (3): C's conversion truncates whatever the mode
// says, which is why codegen selects the chopped form outright.
inline bool fpRounds(unsigned TrapClass) {
  return TrapClass == 1 || TrapClass == 4;
}
// A rounding letter written in the source, on the other hand, does apply to
// float-to-integer: cvttq/svc is the spelling of the chopped, trapping form,
// and dropping the c silently assembled it as cvttq/sv.  Compares (2) and the
// conversions that have no rounding field (5) take none.
inline bool fpTakesWrittenRound(unsigned TrapClass) {
  return fpRounds(TrapClass) || TrapClass == 3;
}
inline StringRef getFPRoundSuffix(unsigned Mode) {
  switch (Mode) {
  case FPRoundChopped:
    return "c";
  case FPRoundMinus:
    return "m";
  case FPRoundDynamic:
    return "d";
  default:
    return StringRef();
  }
}
// The function-field rounding bits (7:6): normal 0x80, chopped 0, minus 0x40,
// dynamic 0xc0.  These replace, rather than add to, the instruction's default.
inline unsigned getFPRoundFuncBits(unsigned Mode) {
  switch (Mode) {
  case FPRoundChopped:
    return 0x000;
  case FPRoundMinus:
    return 0x040;
  case FPRoundDynamic:
    return 0x0c0;
  default:
    return 0x080;
  }
}

// The qualifier a floating-point instruction carries, held in the MCInst's
// flags.  An instruction that came from an assembly file or from the
// disassembler knows its own qualifier -- whatever was written, or whatever the
// bits say -- and Present marks that.  One built by codegen does not, and the
// encoder and printer then derive it from the subtarget, which is where the
// -mieee and -mfp-rounding-mode policy belongs.
// The trap class an FP instruction carries in the low three bits of its
// TSFlags; 0 for one that takes no qualifier.  See TrapClass in
// AlphaInstrFormats.td.
enum : unsigned { TrapClassMask = 0x7 };

enum : unsigned {
  FPQualTrapMask = 0x7ff,
  FPQualRoundShift = 11,
  FPQualRoundMask = 0x3,
  FPQualPresent = 1u << 13,
};

inline unsigned encodeFPQual(unsigned TrapBits, unsigned RoundMode) {
  return FPQualPresent | (TrapBits & FPQualTrapMask) |
         ((RoundMode & FPQualRoundMask) << FPQualRoundShift);
}
inline bool hasFPQual(unsigned Flags) { return Flags & FPQualPresent; }
inline unsigned fpQualTrapBits(unsigned Flags) {
  return Flags & FPQualTrapMask;
}
inline unsigned fpQualRoundMode(unsigned Flags) {
  return (Flags >> FPQualRoundShift) & FPQualRoundMask;
}

// The spelling of a trap qualifier from its function bits.  The v forms differ
// from the u forms only in which instruction carries them, so the caller says
// which family it wants.
inline StringRef getFPTrapSpelling(unsigned Bits, bool IsIntOverflow) {
  switch (Bits) {
  case 0x400:
    return "s";
  case 0x100:
    return IsIntOverflow ? "v" : "u";
  case 0x500:
    return IsIntOverflow ? "sv" : "su";
  case 0x700:
    return IsIntOverflow ? "svi" : "sui";
  default:
    return StringRef();
  }
}
} // namespace Alpha

class Target;
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;

// The FPRoundMode selected by the subtarget's -mfp-rounding-mode features.
unsigned getFPRoundMode(const MCSubtargetInfo &STI);

namespace Alpha {
// The mode argument the OTS X_floating routines take, matching gcc's
// alpha_compute_xfloating_mode_arg.  It goes in the register after the operands
// -- $18 for a routine taking one X_floating value, $20 for the arithmetic,
// which takes two -- so the register is named at each call site rather than
// here.  Round toward +inf is mode 3 and has no -mfp-rounding-mode spelling, so
// it cannot be selected here.
inline unsigned getOtsRoundModeArg(unsigned Mode) {
  switch (Mode) {
  case FPRoundChopped:
    return 0;
  case FPRoundMinus:
    return 1;
  case FPRoundDynamic:
    return 4;
  default:
    return 2; // FPRoundNormal
  }
}
} // namespace Alpha

MCCodeEmitter *createAlphaMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx);
MCAsmBackend *createAlphaAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                    const MCRegisterInfo &MRI,
                                    const MCTargetOptions &Options);
std::unique_ptr<MCObjectTargetWriter> createAlphaELFObjectWriter(uint8_t OSABI);
} // end namespace llvm

// Defines symbolic names for Alpha registers.
#define GET_REGINFO_ENUM
#include "AlphaGenRegisterInfo.inc"

// Defines symbolic names for the Alpha instructions.
#define GET_INSTRINFO_ENUM
#define GET_INSTRINFO_MC_HELPER_DECLS
#include "AlphaGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "AlphaGenSubtargetInfo.inc"

#endif // LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCTARGETDESC_H
