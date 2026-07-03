//===-- AlphaInstrInfo.h - Alpha Instruction Information --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the Alpha implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHAINSTRINFO_H
#define LLVM_LIB_TARGET_ALPHA_ALPHAINSTRINFO_H

#include "AlphaRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "AlphaGenInstrInfo.inc"

namespace llvm {

class AlphaSubtarget;

class AlphaInstrInfo : public AlphaGenInstrInfo {
  const AlphaRegisterInfo RI;

public:
  explicit AlphaInstrInfo(const AlphaSubtarget &STI);

  const AlphaRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAINSTRINFO_H
