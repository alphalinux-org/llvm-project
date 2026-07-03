//===-- AlphaFrameLowering.cpp - Alpha Frame Lowering ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaFrameLowering.h"
#include "AlphaSubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

using namespace llvm;

AlphaFrameLowering::AlphaFrameLowering(const AlphaSubtarget &STI)
    : TargetFrameLowering(StackGrowsDown, Align(16), /*LocalAreaOffset=*/0,
                          Align(16)) {}

void AlphaFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}

void AlphaFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {}

bool AlphaFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  return MF.getFrameInfo().hasVarSizedObjects();
}
