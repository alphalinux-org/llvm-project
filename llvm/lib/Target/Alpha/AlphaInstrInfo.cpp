//===-- AlphaInstrInfo.cpp - Alpha Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaInstrInfo.h"
#include "AlphaSubtarget.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "AlphaGenInstrInfo.inc"

using namespace llvm;

AlphaInstrInfo::AlphaInstrInfo(const AlphaSubtarget &STI)
    : AlphaGenInstrInfo(STI, RI, Alpha::ADJCALLSTACKDOWN,
                        Alpha::ADJCALLSTACKUP),
      RI() {}

void AlphaInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc,
                                 bool RenamableDest, bool RenamableSrc) const {
  if (Alpha::GPRCRegClass.contains(DestReg, SrcReg)) {
    // mov = bis $31, SrcReg, DestReg
    BuildMI(MBB, MI, DL, get(Alpha::BIS), DestReg)
        .addReg(Alpha::R31)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (Alpha::FPRCRegClass.contains(DestReg, SrcReg)) {
    // fmov = cpys SrcReg, SrcReg, DestReg
    BuildMI(MBB, MI, DL, get(Alpha::CPYS), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  llvm_unreachable("Alpha copyPhysReg: unsupported register class");
}

void llvm::addNarrowedMemOperands(MachineInstrBuilder MIB,
                                  const MachineInstr &MI,
                                  MachineMemOperand::Flags Half) {
  MachineFunction &MF = *MIB->getMF();
  for (MachineMemOperand *MMO : MI.memoperands()) {
    auto Flags = (MMO->getFlags() &
                  ~(MachineMemOperand::MOLoad | MachineMemOperand::MOStore)) |
                 Half;
    MIB.addMemOperand(MF.getMachineMemOperand(
        MMO->getPointerInfo(), Flags, MMO->getSize(), MMO->getBaseAlign(),
        MMO->getAAInfo(), MMO->getRanges(), MMO->getSyncScopeID(),
        MMO->getSuccessOrdering(), MMO->getFailureOrdering()));
  }
}

// Describe an access to the whole of a stack slot, so that what follows knows
// which object is touched and can tell one slot's traffic from another's.
static MachineMemOperand *getStackSlotMMO(MachineFunction &MF, int FrameIndex,
                                          MachineMemOperand::Flags Flags) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex), Flags,
      MFI.getObjectSize(FrameIndex), MFI.getObjectAlign(FrameIndex));
}

void AlphaInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned Opc =
      Alpha::GPRCRegClass.hasSubClassEq(RC) ? Alpha::STQ : Alpha::STT;
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBBI, DL, get(Opc))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(
          getStackSlotMMO(MF, FrameIndex, MachineMemOperand::MOStore))
      .setMIFlag(Flags);
}

void AlphaInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          Register DestReg, int FrameIndex,
                                          const TargetRegisterClass *RC,
                                          Register VReg, unsigned SubReg,
                                          MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned Opc =
      Alpha::GPRCRegClass.hasSubClassEq(RC) ? Alpha::LDQ : Alpha::LDT;
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBBI, DL, get(Opc), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(getStackSlotMMO(MF, FrameIndex, MachineMemOperand::MOLoad))
      .setMIFlag(Flags);
}
