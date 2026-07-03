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
#include "llvm/CodeGen/MachineInstrBuilder.h"
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
  report_fatal_error("Alpha copyPhysReg: unsupported register class");
}

void AlphaInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned Opc =
      Alpha::GPRCRegClass.hasSubClassEq(RC) ? Alpha::STQ : Alpha::STT;
  BuildMI(MBB, MBBI, DL, get(Opc))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
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
  BuildMI(MBB, MBBI, DL, get(Opc), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .setMIFlag(Flags);
}

bool AlphaInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();
  if (Opc != Alpha::RMW_STOREI8 && Opc != Alpha::RMW_STOREI16)
    return false;

  // Update one field of the quadword holding it, in place.  This is expanded
  // here, after scheduling, so that nothing lands between the load and the
  // store: an update of another field of the same quadword would be lost.
  bool IsByte = Opc == Alpha::RMW_STOREI8;
  MachineBasicBlock &MBB = *MI.getParent();
  DebugLoc DL = MI.getDebugLoc();
  Register Tmp = MI.getOperand(0).getReg();
  Register Ins = MI.getOperand(1).getReg();
  Register Val = MI.getOperand(2).getReg();
  Register Addr = MI.getOperand(3).getReg();

  BuildMI(MBB, MI, DL, get(Alpha::LDQ_U), Tmp).addReg(Addr);
  BuildMI(MBB, MI, DL, get(IsByte ? Alpha::MSKBL : Alpha::MSKWL), Tmp)
      .addReg(Tmp)
      .addReg(Addr);
  BuildMI(MBB, MI, DL, get(IsByte ? Alpha::INSBL : Alpha::INSWL), Ins)
      .addReg(Val)
      .addReg(Addr);
  BuildMI(MBB, MI, DL, get(Alpha::BIS), Tmp).addReg(Tmp).addReg(Ins);
  BuildMI(MBB, MI, DL, get(Alpha::STQ_U)).addReg(Tmp).addReg(Addr);

  MBB.erase(MI);
  return true;
}
