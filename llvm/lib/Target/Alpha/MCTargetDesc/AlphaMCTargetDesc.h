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
// The floating-point software-completion qualifier letters (without the leading
// '/') for an instruction's trap class under -mieee / -mieee-with-inexact.  The
// class values match TrapClass in AlphaInstrFormats.td.
inline StringRef getFPTrapSuffix(unsigned TrapClass, bool IEEE, bool Inexact) {
  if (!IEEE)
    return StringRef();
  switch (TrapClass) {
  case 1:
    return Inexact ? "sui" : "su"; // Arithmetic.
  case 2:
    return "su"; // Compare (inexact not applicable).
  case 3:
    return Inexact ? "svi" : "sv"; // Float-to-integer.
  case 4:
    return Inexact ? "sui" : ""; // Integer-to-float (inexact only).
  case 5:
    return IEEE ? "s"
                : StringRef(); // S-to-T convert (software completion only).
  default:
    return StringRef();
  }
}

// The amount added to the instruction's function field for the qualifier above:
// 0x500 for su/sv, 0x700 for sui/svi.
inline unsigned getFPTrapFuncBits(unsigned TrapClass, bool IEEE, bool Inexact) {
  StringRef S = getFPTrapSuffix(TrapClass, IEEE, Inexact);
  if (S.empty())
    return 0;
  if (S == "s")
    return 0x400;
  return (S == "sui" || S == "svi") ? 0x700 : 0x500;
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
