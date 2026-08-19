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

#include "llvm/ADT/SmallVector.h"
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
inline FPRoundMode fpQualRoundMode(unsigned Flags) {
  return static_cast<FPRoundMode>((Flags >> FPQualRoundShift) &
                                  FPQualRoundMask);
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
Alpha::FPRoundMode getFPRoundMode(const MCSubtargetInfo &STI);

namespace Alpha {
// One instruction of a constant-materialization sequence.  Each step reads the
// value the step before it left in the destination register -- the first reads
// whatever base the caller starts from, $31 to build a constant out of nothing
// -- so the emitter only has to know how to chain them.  LDAH and LDA add
// Imm << 16 and Imm; SLLi shifts left by Imm and takes its register operand
// first, which is the only shape difference between the three.
struct ConstantStep {
  unsigned Opc; // Alpha::LDAH, Alpha::LDA or Alpha::SLLi
  int64_t Imm;
};

// The steps that build the signed 32-bit value V32 on top of the running
// value.  Always at least one step, so the destination is always written.
void buildConstant32Steps(int32_t V32, SmallVectorImpl<ConstantStep> &Steps);

// The steps that build the 64-bit value V from nothing.  For a value that fits
// in 32 bits this is buildConstant32Steps; anything wider builds the high half,
// shifts it up and adds the low half.
void buildConstantSteps(int64_t V, SmallVectorImpl<ConstantStep> &Steps);

// The extract, insert and mask instructions that operate on a field of the
// given byte width: the low form works on the quadword the field starts in,
// the high form on the one it can run into.  Which pair a caller needs is
// decided by the width alone, and getting the width wrong changes which bytes
// are moved rather than failing to build, so the mapping is written once here
// and read everywhere.
struct FieldOps {
  unsigned ExtL, ExtH, InsL, InsH, MskL, MskH;
};

// Bytes is 1, 2, 4 or 8.  A byte field lies inside one quadword whatever its
// address, so the architecture has no extbh/insbh/mskbh and the high forms are
// zero for that width; no caller of a one-byte field reads them.
FieldOps getFieldOps(unsigned Bytes);
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
