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
  return CSR_Alpha_Call_RegMask;
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
  // SPAdj is non-zero only inside a call sequence of a function whose call
  // frame is not reserved, which here means one with a variable-sized
  // allocation -- and such a function has a frame pointer, so the reference
  // below resolves against $15 and the stack pointer's movement does not
  // reach it.  Assert rather than discard: if it ever is non-zero the offset
  // computed below is wrong by that much, silently.
  assert(SPAdj == 0 && "call-frame adjustment reached eliminateFrameIndex");
  MachineInstr &Inst = *MI;
  MachineFunction &MF = *Inst.getParent()->getParent();
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();

  int FI = Inst.getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  // The displacement operand of the memory instruction follows the base.
  int64_t Offset = TFI->getFrameIndexReference(MF, FI, FrameReg).getFixed() +
                   Inst.getOperand(FIOperandNum + 1).getImm();

  if (!isInt<16>(Offset))
    report_fatal_error("Alpha frame offset does not fit in 16 bits");

  Inst.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*isDef=*/false);
  Inst.getOperand(FIOperandNum + 1).setImm(Offset);
  return false;
}

Register AlphaRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = MF.getSubtarget().getFrameLowering();
  return TFI->hasFP(MF) ? Alpha::R15 : Alpha::R30;
}
