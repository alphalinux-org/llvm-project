//===-- AlphaMCTargetDesc.cpp - Alpha target descriptions -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaMCTargetDesc.h"
#include "AlphaInstPrinter.h"
#include "AlphaMCAsmInfo.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPERS
#include "AlphaGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "AlphaGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "AlphaGenSubtargetInfo.inc"

using namespace llvm;

static MCAsmInfo *createAlphaMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  return new AlphaMCAsmInfo(TT, Options);
}

static MCInstrInfo *createAlphaMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitAlphaMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createAlphaMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitAlphaMCRegisterInfo(X, Alpha::R26);
  return X;
}

static MCSubtargetInfo *
createAlphaMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  if (CPU.empty())
    CPU = "generic";
  return createAlphaMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

static MCInstPrinter *createAlphaMCInstPrinter(const Triple &TT,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  return new AlphaInstPrinter(MAI, MII, MRI);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaTargetMC() {
  Target &T = getTheAlphaTarget();
  TargetRegistry::RegisterMCAsmInfo(T, createAlphaMCAsmInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createAlphaMCInstrInfo);
  TargetRegistry::RegisterMCRegInfo(T, createAlphaMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createAlphaMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createAlphaMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createAlphaMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createAlphaAsmBackend);
}
