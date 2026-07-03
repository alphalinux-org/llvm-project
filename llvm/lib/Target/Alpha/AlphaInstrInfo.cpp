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
  llvm_unreachable("Alpha copyPhysReg: unsupported register class");
}
