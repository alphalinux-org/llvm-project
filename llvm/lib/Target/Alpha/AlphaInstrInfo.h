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
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "AlphaGenInstrInfo.inc"

namespace llvm {

class AlphaSubtarget;

// Give an instruction built out of MI's expansion the memory operands MI
// carried, narrowed to the half of the access it performs.  A read-modify-write
// pseudo claims both a load and a store; the verifier rejects a load handed an
// operand that claims a store, and the other way round.
void addNarrowedMemOperands(MachineInstrBuilder MIB, const MachineInstr &MI,
                            MachineMemOperand::Flags Half);

class AlphaInstrInfo : public AlphaGenInstrInfo {
  const AlphaRegisterInfo RI;

public:
  explicit AlphaInstrInfo(const AlphaSubtarget &STI);

  const AlphaRegisterInfo &getRegisterInfo() const { return RI; }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
      bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
      Register DestReg, int FrameIndex, const TargetRegisterClass *RC,
      Register VReg, unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  // Branch analysis.  Cond is { branch opcode, tested register }.
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;

  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  // Expands the pre-BWX byte/word store, which is held together as one
  // instruction until here so that no other update of the same quadword can be
  // scheduled between its load and its store.
  bool expandPostRAPseudo(MachineInstr &MI) const override;

  // The entry ldgp must stay the function's first instruction: its !gpdisp
  // relocation assumes the procedure value ($27) equals the ldah's address, so
  // nothing may be scheduled in front of it.
  bool isSchedulingBoundary(const MachineInstr &MI,
                            const MachineBasicBlock *MBB,
                            const MachineFunction &MF) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAINSTRINFO_H
