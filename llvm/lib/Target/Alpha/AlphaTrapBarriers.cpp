//===-- AlphaTrapBarriers.cpp - Insert FP trap barriers -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Alpha floating-point traps are imprecise: the hardware may report an
// arithmetic exception several instructions after the one that raised it.  With
// -mtrap-precision=i (and -mieee-conformant), follow every trapping
// floating-point instruction with a trap barrier (trapb) so its exception is
// delivered before execution continues, making the trap precise to that
// instruction.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaSubtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define ALPHA_TRAP_BARRIERS_NAME "Alpha floating-point trap barriers"

namespace {
class AlphaTrapBarriers : public MachineFunctionPass {
public:
  static char ID;
  AlphaTrapBarriers() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const AlphaSubtarget &ST = MF.getSubtarget<AlphaSubtarget>();
    // The 21264 and later report arithmetic exceptions precisely in hardware
    // and treat trapb as a no-op, so no barrier is needed there.
    if (!ST.hasTrapPrecisionInsn() || ST.hasPreciseArithTraps())
      return false;

    const TargetInstrInfo &TII = *ST.getInstrInfo();
    bool Changed = false;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end();
           I != E;) {
        MachineInstr &MI = *I++;
        // The low bits of TSFlags hold the floating-point trap class; a nonzero
        // value marks an instruction that can raise an arithmetic trap.
        if (MI.getDesc().TSFlags & Alpha::TrapClassMask) {
          BuildMI(MBB, I, MI.getDebugLoc(), TII.get(Alpha::TRAPB));
          Changed = true;
        }
      }
    }
    return Changed;
  }

  StringRef getPassName() const override { return ALPHA_TRAP_BARRIERS_NAME; }
};
} // end anonymous namespace

char AlphaTrapBarriers::ID = 0;

INITIALIZE_PASS(AlphaTrapBarriers, "alpha-trap-barriers",
                ALPHA_TRAP_BARRIERS_NAME, false, false)

FunctionPass *llvm::createAlphaTrapBarriers() {
  return new AlphaTrapBarriers();
}
