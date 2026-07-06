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
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
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
  if (isInt<16>(Amount)) {
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDA), Alpha::R30)
        .addImm(Amount)
        .addReg(Alpha::R30);
    return;
  }
  if (!isInt<32>(Amount))
    reportFatalUsageError(
        "Alpha stack frame larger than 2GiB is not supported");
  // Build the amount in the $28 scratch and add it to the stack pointer.
  int64_t Lo = (int16_t)Amount;
  int64_t Hi = (Amount - Lo) >> 16;
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDA), Alpha::R28)
      .addImm(Lo)
      .addReg(Alpha::R31);
  // Hi is never zero here: Amount did not fit in 16 bits, so it differs from
  // its own low half.  It can be 0x8000, which does not fit ldah's signed
  // 16-bit field -- an epilogue that tears down a frame close to 2GiB reaches
  // it -- so split it in two the way eliminateFrameIndex does.
  if (isInt<16>(Hi)) {
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDAH), Alpha::R28)
        .addImm(Hi)
        .addReg(Alpha::R28);
  } else {
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDAH), Alpha::R28)
        .addImm(Hi / 2)
        .addReg(Alpha::R28);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDAH), Alpha::R28)
        .addImm(Hi / 2)
        .addReg(Alpha::R28);
  }
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::ADDQ), Alpha::R30)
      .addReg(Alpha::R30)
      .addReg(Alpha::R28);
}

static void copyReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                    const DebugLoc &DL, const AlphaInstrInfo &TII, Register Dst,
                    Register Src) {
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::BIS), Dst)
      .addReg(Alpha::R31)
      .addReg(Src);
}

// Skip past the callee-save spills the prolog/epilog inserter placed at the top
// of the entry block before this runs.  It does not flag them, so they are
// recognized by the frame indices they address; the frame index operands are
// still there, since they are replaced by a later part of the same pass.
static MachineBasicBlock::iterator
skipCSRSpills(MachineBasicBlock::iterator MBBI, MachineBasicBlock &MBB,
              const MachineFrameInfo &MFI) {
  auto IsCSRSpill = [&](const MachineInstr &MI) {
    if (!MI.mayStore())
      return false;
    for (const MachineOperand &MO : MI.operands())
      if (MO.isFI())
        for (const CalleeSavedInfo &CSI : MFI.getCalleeSavedInfo())
          if (CSI.getFrameIdx() == MO.getIndex())
            return true;
    return false;
  };
  while (MBBI != MBB.end() && IsCSRSpill(*MBBI))
    ++MBBI;
  return MBBI;
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

  // A branch's 21-bit displacement reaches +/- 4 MiB.  A function that large
  // may need branch relaxation, which materializes a far target's address gp-
  // relatively; that only works if the global pointer is established in the
  // prologue.  Relaxation runs after prologue insertion, too late to request
  // it, so ask for the global pointer here once the code approaches the branch
  // range.
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  uint64_t Size = 0;
  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB)
      Size += TII.getInstSizeInBytes(MI);
    if (Size > (3u << 20))
      break;
  }
  if (Size > (3u << 20))
    MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();
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
  // This happens under -msmall-text too: a caller reaching us with a br rather
  // than a jsr aims the branch past these two instructions (R_ALPHA_BRSGP), so
  // they cost that caller nothing, and a caller from another global-pointer
  // region -- libc calling main, or invoking a callback we handed it -- gets a
  // correct $29 only because they are here.
  if (AFI->usesGP()) {
    MBB.addLiveIn(Alpha::R27);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDGP));
  }

  adjustStack(MBB, MBBI, DL, TII, -(int64_t)StackSize);

  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

  // Insert a CFI directive at Pos, which is the position *after* the
  // instruction whose effect it describes, so an asynchronous unwind from any
  // point in the prologue observes the correct state.
  auto emitCFI = [&](MachineBasicBlock::iterator Pos,
                     const MCCFIInstruction &Inst) {
    unsigned Idx = MF.addFrameInst(Inst);
    BuildMI(MBB, Pos, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(Idx)
        .setMIFlag(MachineInstr::FrameSetup);
  };

  if (StackSize)
    emitCFI(MBBI, MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));

  if (hasFP(MF)) {
    // Save the caller's $15 and set $15 to the current stack pointer, before
    // the callee-save spills rather than after them: those address their slots
    // through $15, which stays put while variable stack allocations move $30.
    // The save itself must address its slot through $30, since $15 is not the
    // frame pointer yet.
    int FPSlot = AFI->getFramePointerSaveIndex();
    int64_t FPOff = MFI.getObjectOffset(FPSlot);
    unsigned DwarfFP = TRI->getDwarfRegNum(Alpha::R15, true);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::STQ))
        .addReg(Alpha::R15)
        .addReg(Alpha::R30)
        .addImm(FPOff + (int64_t)StackSize)
        .setMIFlag(MachineInstr::FrameSetup);
    emitCFI(MBBI, MCCFIInstruction::createOffset(nullptr, DwarfFP, FPOff));
    copyReg(MBB, MBBI, DL, TII, Alpha::R15, Alpha::R30);
    emitCFI(MBBI, MCCFIInstruction::createDefCfaRegister(nullptr, DwarfFP));
  }

  // Describe each callee-saved register once they have all been stored.  A
  // rule that arrives late is still correct -- until it does, the unwinder
  // reads the value out of the register, which still holds it -- but one that
  // arrives early sends it to a slot that has not been written.
  MachineBasicBlock::iterator Pos = skipCSRSpills(MBBI, MBB, MFI);
  for (const CalleeSavedInfo &CSI : MFI.getCalleeSavedInfo())
    emitCFI(Pos, MCCFIInstruction::createOffset(
                     nullptr, TRI->getDwarfRegNum(CSI.getReg(), true),
                     MFI.getObjectOffset(CSI.getFrameIdx())));
}

void AlphaFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  auto *AFI = MF.getInfo<AlphaMachineFunctionInfo>();
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  bool NeedsCFI =
      StackSize != 0 || hasFP(MF) || !MFI.getCalleeSavedInfo().empty();

  // Insert a CFI directive at Pos (which is the position *after* the
  // instruction whose effect it describes), so an asynchronous unwind from any
  // point in the epilogue observes the correct state.
  auto insertCFI = [&](MachineBasicBlock::iterator Pos,
                       const MCCFIInstruction &Inst) {
    if (!NeedsCFI)
      return;
    unsigned Idx = MF.addFrameInst(Inst);
    BuildMI(MBB, Pos, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(Idx)
        .setMIFlag(MachineInstr::FrameDestroy);
  };

  if (hasFP(MF)) {
    // Restore $30 from the frame pointer (undoing any variable allocation),
    // then restore the caller's $15 and deallocate the fixed frame.  All of
    // that follows the callee-saved reloads, which PEI placed above this point
    // and which still reach their slots through $15.
    copyReg(MBB, MBBI, DL, TII, Alpha::R30, Alpha::R15);
    // $30 now holds the frame bottom (still equal to $15), so re-anchor the CFA
    // to $30 before $15 is clobbered by its reload.
    insertCFI(MBBI,
              MCCFIInstruction::cfiDefCfa(
                  nullptr, TRI->getDwarfRegNum(Alpha::R30, true), StackSize));

    // $30 now equals the frame bottom, so reach the save slot through it (the
    // caller's $15 is about to be restored, so it cannot be the base).
    int FPSlot = AFI->getFramePointerSaveIndex();
    int64_t Off =
        MF.getFrameInfo().getObjectOffset(FPSlot) + (int64_t)StackSize;
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDQ), Alpha::R15)
        .addReg(Alpha::R30)
        .addImm(Off);
    insertCFI(MBBI, MCCFIInstruction::createRestore(
                        nullptr, TRI->getDwarfRegNum(Alpha::R15, true)));
  }

  // Mark each callee-saved register restored.  Prologue/epilogue insertion puts
  // the reloads immediately before this point, so one position after all of
  // them describes every one: a .cfi_restore that arrives late is still correct
  // -- until it does, the unwinder reads the save slot, which still holds the
  // value -- whereas searching backwards for a definition of the register
  // matches any definition rather than the reload, and finds none at all when
  // there is no reload to find.
  for (const CalleeSavedInfo &CSI : MFI.getCalleeSavedInfo())
    insertCFI(MBBI, MCCFIInstruction::createRestore(
                        nullptr, TRI->getDwarfRegNum(CSI.getReg(), true)));

  adjustStack(MBB, MBBI, DL, TII, StackSize);
  // The frame is gone: the CFA is the stack pointer with no offset.
  insertCFI(MBBI, MCCFIInstruction::cfiDefCfaOffset(nullptr, 0));
}

void AlphaFrameLowering::resetCFIToInitialState(MachineBasicBlock &MBB) const {
  // Emit the CFI that returns the unwind state to what it was at function
  // entry: the CFA is the stack pointer, and every callee-saved register holds
  // its original value.  The CFIFixup pass uses this when a block that follows
  // an epilogue in layout order still needs the no-frame state.
  MachineFunction &MF = *MBB.getParent();
  const AlphaInstrInfo &TII = *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  DebugLoc DL;
  auto emitCFI = [&](const MCCFIInstruction &Inst) {
    unsigned Idx = MF.addFrameInst(Inst);
    BuildMI(MBB, MBB.begin(), DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(Idx);
  };
  emitCFI(MCCFIInstruction::cfiDefCfa(
      nullptr, TRI->getDwarfRegNum(Alpha::R30, true), 0));
  for (const CalleeSavedInfo &CSI : MF.getFrameInfo().getCalleeSavedInfo())
    emitCFI(MCCFIInstruction::createSameValue(
        nullptr, TRI->getDwarfRegNum(CSI.getReg(), true)));
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
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  // A frame pointer is needed when the stack pointer moves during the function,
  // and when the frame address is taken, since that address then has to name
  // something that does not move.  It is also needed whenever the user asks for
  // one: -fno-omit-frame-pointer reaches here as DisableFramePointerElim, and
  // without this test the option does nothing at all on Alpha.
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}
