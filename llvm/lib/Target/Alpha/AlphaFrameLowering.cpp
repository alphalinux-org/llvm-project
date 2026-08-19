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

// Copy one integer register to another with `bis $31, Src, Dst`.
static void copyReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, const AlphaInstrInfo &TII, Register Dst,
                    Register Src) {
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::BIS), Dst)
      .addReg(Alpha::R31)
      .addReg(Src);
}

void AlphaFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // When a frame pointer is needed, reserve a slot for the caller's $15, which
  // the prologue saves before repurposing $15 as the frame pointer.
  if (hasFP(MF)) {
    auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
    if (FI->getFramePointerSaveIndex() < 0)
      FI->setFramePointerSaveIndex(MF.getFrameInfo().CreateStackObject(
          8, Align(8), /*isSpillSlot=*/true));
  }
}

void AlphaFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *AFI = MF.getInfo<AlphaMachineFunctionInfo>();
  uint64_t StackSize = MFI.getStackSize();
  MachineBasicBlock::iterator MBBI = MBB.begin();
  DebugLoc DL;

  // Establish the global pointer from the procedure value ($27) if needed.
  if (AFI->usesGP()) {
    MBB.addLiveIn(Alpha::R27);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDGP));
  }

  adjustStack(MBB, MBBI, DL, TII, -(int64_t)StackSize);

  if (hasFP(MF)) {
    // Save the caller's $15 and set $15 to the current stack pointer, before
    // the callee-save spills rather than after them: those address their slots
    // through $15, which stays put while variable stack allocations move $30.
    // The save itself must address its slot through $30, since $15 is not the
    // frame pointer yet.
    MachineBasicBlock::iterator Save = MBBI;
    int FPSlot = AFI->getFramePointerSaveIndex();
    int64_t Off = MFI.getObjectOffset(FPSlot) + (int64_t)StackSize;
    BuildMI(MBB, Save, DL, TII.get(Alpha::STQ))
        .addReg(Alpha::R15)
        .addReg(Alpha::R30)
        .addImm(Off);
    copyReg(MBB, Save, DL, TII, Alpha::R15, Alpha::R30);
  }
}

void AlphaFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  auto *AFI = MF.getInfo<AlphaMachineFunctionInfo>();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  if (hasFP(MF)) {
    // Restore $30 from the frame pointer (undoing any variable allocation),
    // then restore the caller's $15 and deallocate the fixed frame.  All of
    // that follows the callee-saved reloads, which PEI placed above this point
    // and which still reach their slots through $15.
    copyReg(MBB, MBBI, DL, TII, Alpha::R30, Alpha::R15);

    // $30 now equals the frame bottom, so reach the save slot through it (the
    // caller's $15 is about to be restored, so it cannot be the base).
    int FPSlot = AFI->getFramePointerSaveIndex();
    int64_t Off =
        MF.getFrameInfo().getObjectOffset(FPSlot) + (int64_t)StackSize;
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDQ), Alpha::R15)
        .addReg(Alpha::R30)
        .addImm(Off);
  }

  adjustStack(MBB, MBBI, DL, TII, StackSize);
}

bool AlphaFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  // A call passes its stack arguments at fixed offsets from the stack pointer,
  // so the space for the largest of them is part of the frame and no call
  // moves the stack pointer.  The default answer would be no as soon as the
  // function has a frame pointer, which would leave that space uncounted and
  // let a spill slot land on top of an outgoing argument.
  //
  // A variable-sized allocation is the exception: it moves the stack pointer,
  // so the argument area cannot keep a fixed place in the frame and has to be
  // carved out around each call instead.
  return !MF.getFrameInfo().hasVarSizedObjects();
}

MachineBasicBlock::iterator AlphaFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  // With the call frame reserved on entry, the markers just get removed.
  if (!hasReservedCallFrame(MF)) {
    // Otherwise make room for the outgoing arguments below whatever the stack
    // pointer now points at, and take it back afterwards, keeping the stack
    // pointer aligned across the call.
    const AlphaInstrInfo &TII =
        *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
    int64_t Amount = alignTo(I->getOperand(0).getImm(), getStackAlign());
    if (Amount) {
      if (I->getOpcode() == TII.getCallFrameSetupOpcode())
        Amount = -Amount;
      adjustStack(MBB, I, I->getDebugLoc(), TII, Amount);
    }
  }
  return MBB.erase(I);
}

bool AlphaFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects();
}
