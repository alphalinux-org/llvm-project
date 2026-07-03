//===-- AlphaRegisterInfo.cpp - Alpha Register Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaRegisterInfo.h"
#include "Alpha.h"
#include "AlphaFrameLowering.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "AlphaGenRegisterInfo.inc"

using namespace llvm;

AlphaRegisterInfo::AlphaRegisterInfo()
    : AlphaGenRegisterInfo(/*RA=*/Alpha::R26) {}

const MCPhysReg *
AlphaRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_Alpha_SaveList;
}

const uint32_t *
AlphaRegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                        CallingConv::ID) const {
  return CSR_Alpha_RegMask;
}

BitVector AlphaRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // Hardwired-zero registers.
  Reserved.set(Alpha::R31);
  Reserved.set(Alpha::F31);
  // Stack pointer and global pointer.
  Reserved.set(Alpha::R30);
  Reserved.set(Alpha::R29);
  // $15 is the frame pointer only in a function that sets one up; anywhere else
  // it is an ordinary callee-saved register, and reserving it there would lose
  // one for nothing.
  if (MF.getSubtarget().getFrameLowering()->hasFP(MF))
    Reserved.set(Alpha::R15);
  return Reserved;
}

bool AlphaRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  report_fatal_error("Alpha frame index elimination is not yet implemented");
}

Register AlphaRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  return TFI->hasFP(MF) ? Alpha::R15 : Alpha::R30;
}
