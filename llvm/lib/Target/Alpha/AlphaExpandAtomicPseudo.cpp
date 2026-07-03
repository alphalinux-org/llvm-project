//===-- AlphaExpandAtomicPseudo.cpp - Expand atomic pseudo instructions ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Expand the pseudo instructions that stand for an ldq_l/stq_c retry loop.
//
// The Alpha Architecture Handbook (5.5.2, "Using Load Locked and Store
// Conditional") requires that no memory operation appear between the load
// locked and the store conditional: a processor is only obliged to make
// forward progress for a sequence that has none, and on real hardware an
// intervening access clears the lock flag, so the store conditional fails
// every time round and the loop spins forever.
//
// That rules out building these loops before register allocation, which is
// free to place a spill or a reload inside the window -- at -O0 it always
// does, and the resulting binary hangs on the first atomic it executes.  So
// the loops are one instruction until here: this pass runs from
// addPreEmitPass, after allocation, after the prologue and epilogue are in
// place and after the post-RA scheduler, which is the last point at which
// anything could have been inserted or moved.  Each pseudo names the scratch
// registers its expansion needs as extra outputs, so the allocator, and not
// this pass, is what decides which registers those are.
//
// Because the expansion happens this late, each pseudo also has to declare the
// size of what it expands to: PrologEpilogInserter adds those up long before
// this runs, to decide whether the function is close enough to the 21-bit
// branch range to need a global pointer for relaxation.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaInstrInfo.h"
#include "AlphaSubtarget.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define ALPHA_EXPAND_ATOMIC_PSEUDO_NAME                                        \
  "Alpha atomic pseudo instruction expansion pass"

namespace {

class AlphaExpandAtomicPseudo : public MachineFunctionPass {
public:
  const AlphaSubtarget *STI = nullptr;
  const AlphaInstrInfo *TII = nullptr;
  static char ID;

  AlphaExpandAtomicPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return ALPHA_EXPAND_ATOMIC_PSEUDO_NAME;
  }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);

  bool expandAtomicRMW(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                       MachineBasicBlock::iterator &NextMBBI);
  bool expandAtomicCmpXchg(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI);
  bool expandSafeStore(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                       MachineBasicBlock::iterator &NextMBBI);
};

char AlphaExpandAtomicPseudo::ID = 0;

bool AlphaExpandAtomicPseudo::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<AlphaSubtarget>();
  TII = STI->getInstrInfo();

  bool Modified = false;
  for (MachineBasicBlock &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool AlphaExpandAtomicPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }
  return Modified;
}

bool AlphaExpandAtomicPseudo::expandMI(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       MachineBasicBlock::iterator &NextMBBI) {
  // AlphaInstrInfo::getInstSizeInBytes reports the Size field of the pseudo,
  // so each of these has to declare the size of what it becomes here.
  switch (MBBI->getOpcode()) {
  case Alpha::ATOMIC_RMW_LOOP:
    return expandAtomicRMW(MBB, MBBI, NextMBBI);
  case Alpha::ATOMIC_CAS_LOOP:
    return expandAtomicCmpXchg(MBB, MBBI, NextMBBI);
  case Alpha::SAFE_STORE_LOOP:
    return expandSafeStore(MBB, MBBI, NextMBBI);
  }
  return false;
}

// Create Count blocks after MBB, move everything after MI into the last of
// them, and hand the caller the new blocks.  The successors of MBB move with
// the instructions, so the exit block ends up with them.
static void splitBlock(MachineBasicBlock &MBB, MachineInstr &MI,
                       SmallVectorImpl<MachineBasicBlock *> &New,
                       unsigned Count) {
  MachineFunction *MF = MBB.getParent();
  MachineFunction::iterator Ins = ++MBB.getIterator();
  for (unsigned I = 0; I != Count; ++I) {
    MachineBasicBlock *B = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
    MF->insert(Ins, B);
    New.push_back(B);
  }
  MachineBasicBlock *Exit = New.back();
  Exit->splice(Exit->end(), &MBB, MI.getIterator(), MBB.end());
  Exit->transferSuccessorsAndUpdatePHIs(&MBB);
}

static void addLiveIns(ArrayRef<MachineBasicBlock *> Blocks) {
  LivePhysRegs LiveRegs;
  for (MachineBasicBlock *B : Blocks)
    computeAndAddLiveIns(LiveRegs, *B);
}

// ldq_l   Dst, 0(Addr)
// <op>    New, Dst, Val      (bis $31, Val for an exchange)
// stq_c   New, 0(Addr)       ; New <- success
// beq     New, LoopBB
bool AlphaExpandAtomicPseudo::expandAtomicRMW(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  const DebugLoc &DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  Register New = MI.getOperand(1).getReg();
  Register Addr = MI.getOperand(2).getReg();
  Register Val = MI.getOperand(3).getReg();
  // Zero for an exchange, which just writes the operand back.
  unsigned Opc = MI.getOperand(4).getImm();
  bool Is32 = MI.getOperand(5).getImm() != 0;
  unsigned LLOpc = Is32 ? Alpha::LDL_L : Alpha::LDQ_L;
  unsigned SCOpc = Is32 ? Alpha::STL_C : Alpha::STQ_C;

  SmallVector<MachineBasicBlock *, 2> Blocks;
  splitBlock(MBB, MI, Blocks, 2);
  MachineBasicBlock *LoopBB = Blocks[0], *ExitBB = Blocks[1];
  MBB.addSuccessor(LoopBB);
  LoopBB->addSuccessor(LoopBB);
  LoopBB->addSuccessor(ExitBB);

  MachineInstrBuilder MIB =
      BuildMI(LoopBB, DL, TII->get(LLOpc), Dst).addReg(Addr).addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOLoad);

  if (Opc)
    BuildMI(LoopBB, DL, TII->get(Opc), New).addReg(Dst).addReg(Val);
  else
    BuildMI(LoopBB, DL, TII->get(Alpha::BIS), New)
        .addReg(Alpha::R31)
        .addReg(Val);

  // stq_c overwrites its source with the success flag, so the loop needs no
  // register of its own for it.
  MIB = BuildMI(LoopBB, DL, TII->get(SCOpc), New)
            .addReg(New)
            .addReg(Addr)
            .addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOStore);
  BuildMI(LoopBB, DL, TII->get(Alpha::BEQ)).addReg(New).addMBB(LoopBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  addLiveIns(Blocks);
  return true;
}

// LoopBB:  ldq_l  Dst, 0(Addr)          ; ldl_l for a longword
//          addl   Cmp32, $31, Cmp       ; longword only, to match ldl_l
//          cmpeq  Tmp, Dst, Cmp
//          beq    Tmp, ExitBB           ; mismatch: leave Dst as read
// StoreBB: bis    Tmp, $31, New
//          stq_c  Tmp, 0(Addr)          ; Tmp <- success
//          beq    Tmp, LoopBB
bool AlphaExpandAtomicPseudo::expandAtomicCmpXchg(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  const DebugLoc &DL = MI.getDebugLoc();

  Register Dst = MI.getOperand(0).getReg();
  Register Tmp = MI.getOperand(1).getReg();
  Register Cmp32 = MI.getOperand(2).getReg();
  Register Addr = MI.getOperand(3).getReg();
  Register Cmp = MI.getOperand(4).getReg();
  Register New = MI.getOperand(5).getReg();
  bool Is32 = MI.getOperand(6).getImm() != 0;
  assert(!Is32 && "no longword load locked/store conditional yet");
  unsigned LLOpc = Alpha::LDQ_L;
  unsigned SCOpc = Alpha::STQ_C;

  SmallVector<MachineBasicBlock *, 3> Blocks;
  splitBlock(MBB, MI, Blocks, 3);
  MachineBasicBlock *LoopBB = Blocks[0], *StoreBB = Blocks[1],
                    *ExitBB = Blocks[2];
  MBB.addSuccessor(LoopBB);
  LoopBB->addSuccessor(StoreBB);
  LoopBB->addSuccessor(ExitBB);
  StoreBB->addSuccessor(LoopBB);
  StoreBB->addSuccessor(ExitBB);

  MachineInstrBuilder MIB =
      BuildMI(LoopBB, DL, TII->get(LLOpc), Dst).addReg(Addr).addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOLoad);

  // ldl_l sign-extends the longword it read, so sign-extend the expected value
  // too or the comparison sees bits that differ above bit 31.
  Register CmpVal = Cmp;
  if (Is32) {
    BuildMI(LoopBB, DL, TII->get(Alpha::ADDL), Cmp32)
        .addReg(Alpha::R31)
        .addReg(Cmp);
    CmpVal = Cmp32;
  }
  BuildMI(LoopBB, DL, TII->get(Alpha::CMPEQ), Tmp).addReg(Dst).addReg(CmpVal);
  BuildMI(LoopBB, DL, TII->get(Alpha::BEQ)).addReg(Tmp).addMBB(ExitBB);

  BuildMI(StoreBB, DL, TII->get(Alpha::BIS), Tmp)
      .addReg(Alpha::R31)
      .addReg(New);
  MIB = BuildMI(StoreBB, DL, TII->get(SCOpc), Tmp)
            .addReg(Tmp)
            .addReg(Addr)
            .addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOStore);
  BuildMI(StoreBB, DL, TII->get(Alpha::BEQ)).addReg(Tmp).addMBB(LoopBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  addLiveIns(Blocks);
  return true;
}

//          bic    Aligned, Addr, 7
//          ins    Ins, Val, Addr
// LoopBB:  ldq_l  Old, 0(Aligned)
//          msk    Old, Old, Addr
//          bis    Old, Ins, Old
//          stq_c  Old, 0(Aligned)      ; Old <- success
//          beq    Old, LoopBB
bool AlphaExpandAtomicPseudo::expandSafeStore(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineInstr &MI = *MBBI;
  const DebugLoc &DL = MI.getDebugLoc();

  Register Aligned = MI.getOperand(0).getReg();
  Register Ins = MI.getOperand(1).getReg();
  Register Old = MI.getOperand(2).getReg();
  Register Val = MI.getOperand(3).getReg();
  Register Addr = MI.getOperand(4).getReg();
  bool IsWord = MI.getOperand(5).getImm() != 0;
  unsigned MskOpc = IsWord ? Alpha::MSKWL : Alpha::MSKBL;
  unsigned InsOpc = IsWord ? Alpha::INSWL : Alpha::INSBL;

  BuildMI(MBB, MI, DL, TII->get(Alpha::BICi), Aligned).addReg(Addr).addImm(7);
  BuildMI(MBB, MI, DL, TII->get(InsOpc), Ins).addReg(Val).addReg(Addr);

  SmallVector<MachineBasicBlock *, 2> Blocks;
  splitBlock(MBB, MI, Blocks, 2);
  MachineBasicBlock *LoopBB = Blocks[0], *ExitBB = Blocks[1];
  MBB.addSuccessor(LoopBB);
  LoopBB->addSuccessor(LoopBB);
  LoopBB->addSuccessor(ExitBB);

  MachineInstrBuilder MIB =
      BuildMI(LoopBB, DL, TII->get(Alpha::LDQ_L), Old)
          .addReg(Aligned)
          .addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOLoad);
  BuildMI(LoopBB, DL, TII->get(MskOpc), Old).addReg(Old).addReg(Addr);
  BuildMI(LoopBB, DL, TII->get(Alpha::BIS), Old).addReg(Ins).addReg(Old);
  MIB = BuildMI(LoopBB, DL, TII->get(Alpha::STQ_C), Old)
            .addReg(Old)
            .addReg(Aligned)
            .addImm(0);
  addNarrowedMemOperands(MIB, MI, MachineMemOperand::MOStore);
  BuildMI(LoopBB, DL, TII->get(Alpha::BEQ)).addReg(Old).addMBB(LoopBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();
  addLiveIns(Blocks);
  return true;
}

} // end anonymous namespace

INITIALIZE_PASS(AlphaExpandAtomicPseudo, "alpha-expand-atomic-pseudo",
                ALPHA_EXPAND_ATOMIC_PSEUDO_NAME, false, false)

FunctionPass *llvm::createAlphaExpandAtomicPseudo() {
  return new AlphaExpandAtomicPseudo();
}
