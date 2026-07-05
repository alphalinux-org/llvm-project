//===-- AlphaSubtarget.cpp - Alpha Subtarget Information ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"

#define DEBUG_TYPE "alpha-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "AlphaGenSubtargetInfo.inc"

using namespace llvm;

AlphaSubtarget &AlphaSubtarget::initializeSubtargetDependencies(StringRef CPU,
                                                                StringRef FS) {
  if (CPU.empty())
    CPU = "generic";
  ParseSubtargetFeatures(CPU, /*TuneCPU=*/CPU, FS);
  return *this;
}

AlphaSubtarget::AlphaSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                               const TargetMachine &TM)
    : AlphaGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      ReserveRegister(TM.getMCRegisterInfo().getNumRegs()),
      InstrInfo(initializeSubtargetDependencies(CPU, FS)),
      TLInfo(static_cast<const AlphaTargetMachine &>(TM), *this),
      FrameLowering(*this) {}
