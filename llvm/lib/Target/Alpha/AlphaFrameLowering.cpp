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
    report_fatal_error("Alpha stack frame larger than 2GiB is not supported");
  // Build the amount in the $28 scratch and add it to the stack pointer.
  int64_t Lo = (int16_t)Amount;
  int64_t Hi = (Amount - Lo) >> 16;
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDA), Alpha::R28)
      .addImm(Lo)
      .addReg(Alpha::R31);
  // Hi is never zero here: Amount did not fit in 16 bits, so it differs from
  // its own low half.
  BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDAH), Alpha::R28)
      .addImm(Hi)
      .addReg(Alpha::R28);
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

// Skip past the frame-setup (or, with IsSetup=false, frame-destroy) callee-save
// spills/reloads that the prolog/epilog inserter placed in the block.
static MachineBasicBlock::iterator
skipFrameInstrs(MachineBasicBlock::iterator MBBI, MachineBasicBlock &MBB,
                bool IsSetup) {
  auto Flag = IsSetup ? MachineInstr::FrameSetup : MachineInstr::FrameDestroy;
  while (MBBI != MBB.end() && MBBI->getFlag(Flag))
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

// Re-establish $29 at the top of every landing pad; see LDGPself in
// AlphaInstrInfo.td for why one is needed.  The reload must come before
// anything else in the block but after the label the LSDA points at, or the
// unwinder lands past it.
static void emitEHPadGPReloads(MachineFunction &MF, const AlphaInstrInfo &TII) {
  for (MachineBasicBlock &MBB : MF) {
    if (!MBB.isEHPad())
      continue;
    MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
    while (I != E && (I->isEHLabel() || I->isCFIInstruction()))
      ++I;
    BuildMI(MBB, I, DebugLoc(), TII.get(Alpha::LDGPself));
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
  // This happens under -msmall-text too: a caller reaching us with a br rather
  // than a jsr aims the branch past these two instructions (R_ALPHA_BRSGP), so
  // they cost that caller nothing, and a caller from another global-pointer
  // region -- libc calling main, or invoking a callback we handed it -- gets a
  // correct $29 only because they are here.
  if (AFI->usesGP()) {
    MBB.addLiveIn(Alpha::R27);
    BuildMI(MBB, MBBI, DL, TII.get(Alpha::LDGP));
    emitEHPadGPReloads(MF, TII);
  }

  adjustStack(MBB, MBBI, DL, TII, -(int64_t)StackSize);

  // Position for the frame-setup save and the CFI, after the callee-save
  // spills.
  MachineBasicBlock::iterator Pos =
      skipFrameInstrs(MBBI, MBB, /*IsSetup=*/true);
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();

  auto emitCFI = [&](const MCCFIInstruction &Inst) {
    unsigned Idx = MF.addFrameInst(Inst);
    BuildMI(MBB, Pos, DL, TII.get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(Idx)
        .setMIFlag(MachineInstr::FrameSetup);
  };

  if (hasFP(MF)) {
    // After the frame is allocated and the callee-saved registers spilled, save
    // the caller's $15 and set $15 to the current stack pointer.  Later
    // references to fixed frame slots use $15, which stays put while variable
    // stack allocations move $30.  The save itself must address the slot
    // through $30, since $15 is not the frame pointer yet.
    int FPSlot = AFI->getFramePointerSaveIndex();
    int64_t FPOff = MFI.getObjectOffset(FPSlot);
    BuildMI(MBB, Pos, DL, TII.get(Alpha::STQ))
        .addReg(Alpha::R15)
        .addReg(Alpha::R30)
        .addImm(FPOff + (int64_t)StackSize)
        .setMIFlag(MachineInstr::FrameSetup);
    copyReg(MBB, Pos, DL, TII, Alpha::R15, Alpha::R30);
  }

  // Describe the frame for the unwinder: the CFA offset, each saved register,
  // and, with a frame pointer, that the CFA is now anchored to $15.
  if (StackSize)
    emitCFI(MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));
  for (const CalleeSavedInfo &CSI : MFI.getCalleeSavedInfo())
    emitCFI(MCCFIInstruction::createOffset(
        nullptr, TRI->getDwarfRegNum(CSI.getReg(), true),
        MFI.getObjectOffset(CSI.getFrameIdx())));
  if (hasFP(MF)) {
    unsigned DwarfFP = TRI->getDwarfRegNum(Alpha::R15, true);
    emitCFI(MCCFIInstruction::createOffset(
        nullptr, DwarfFP,
        MFI.getObjectOffset(AFI->getFramePointerSaveIndex())));
    emitCFI(MCCFIInstruction::createDefCfaRegister(nullptr, DwarfFP));
  }
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
    // then, after the callee-saved reloads, restore the caller's $15 and
    // deallocate the fixed frame.  The reloads run first so they can still use
    // $15 to reach the fixed slots.
    MachineBasicBlock::iterator First = MBB.getFirstTerminator();
    while (First != MBB.begin() &&
           std::prev(First)->getFlag(MachineInstr::FrameDestroy))
      --First;
    copyReg(MBB, First, DL, TII, Alpha::R30, Alpha::R15);
    // $30 now holds the frame bottom (still equal to $15), so re-anchor the CFA
    // to $30 before $15 is clobbered by its reload.
    insertCFI(First,
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

  // Mark each callee-saved register restored immediately after its reload,
  // found as the last definition of the register before the terminator.
  for (const CalleeSavedInfo &CSI : MFI.getCalleeSavedInfo()) {
    Register Reg = CSI.getReg();
    for (MachineBasicBlock::iterator It = MBBI; It != MBB.begin();) {
      --It;
      if (It->definesRegister(Reg, TRI)) {
        insertCFI(std::next(It), MCCFIInstruction::createRestore(
                                     nullptr, TRI->getDwarfRegNum(Reg, true)));
        break;
      }
    }
  }

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
  // something that does not move.
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}
