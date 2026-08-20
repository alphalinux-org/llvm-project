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

#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {
class Target;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;

MCCodeEmitter *createAlphaMCCodeEmitter(const MCInstrInfo &MCII,
                                        MCContext &Ctx);
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
