//===-- AlphaFrameLowering.cpp - Alpha Frame Lowering ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFrameLowering.h"
#include "AlphaInstrInfo.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

AlphaFrameLowering::AlphaFrameLowering(const AlphaSubtarget &STI)
    : TargetFrameLowering(StackGrowsDown, Align(16), /*LocalAreaOffset=*/0,
                          Align(16)) {}

// Adjust the stack pointer by Amount using `lda $sp, Amount($sp)`.  Amount must
// fit in the 16-bit signed displacement; larger frames are not handled yet.
static void adjustStack(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI, const DebugLoc &DL,
                        const AlphaInstrInfo &TII, int64_t Amount) {
  if (Amount == 0)
    return;
  if (!isInt<16>(Amount))
    report_fatal_error("Alpha stack frame larger than 32KiB is not supported");
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDA), Alpha::R30)
      .addImm(Amount)
      .addReg(Alpha::R30);
}

void AlphaFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  // Establish the global pointer from the procedure value ($27) if needed.
  if (MF.getInfo<AlphaMachineFunctionInfo>()->usesGP()) {
    MBB.addLiveIn(Alpha::R27);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDGP));
  }

  adjustStack(MBB, MBBI, DL, TII, -(int64_t)StackSize);
}

void AlphaFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  adjustStack(MBB, MBBI, DL, TII, StackSize);
}

MachineBasicBlock::iterator AlphaFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // Arguments are passed in registers, so the call-frame markers just get
  // removed; stack argument space is not handled yet.
  return MBB.erase(I);
}

bool AlphaFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects();
}
