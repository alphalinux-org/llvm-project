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
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPERS
// Alpha_MC::verifyInstructionPredicates, which AlphaAsmPrinter calls on every
// instruction it emits so that an instruction gated on a subtarget feature
// cannot reach a subtarget without it.  The body compiles away when NDEBUG is
// set.
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "AlphaGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "AlphaGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "AlphaGenSubtargetInfo.inc"

using namespace llvm;

unsigned llvm::getFPRoundMode(const MCSubtargetInfo &STI) {
  if (STI.hasFeature(Alpha::FeatureFPRoundChopped))
    return Alpha::FPRoundChopped;
  if (STI.hasFeature(Alpha::FeatureFPRoundMinus))
    return Alpha::FPRoundMinus;
  if (STI.hasFeature(Alpha::FeatureFPRoundDynamic))
    return Alpha::FPRoundDynamic;
  return Alpha::FPRoundNormal;
}

static MCAsmInfo *createAlphaMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new AlphaMCAsmInfo(TT, Options);
  // At function entry the canonical frame address is the stack pointer.
  unsigned SP = MRI.getDwarfRegNum(Alpha::R30, /*isEH=*/true);
  MAI->addInitialFrameState(MCCFIInstruction::cfiDefCfa(nullptr, SP, 0));
  return MAI;
}

// Where a branch goes, so a disassembler can name its target.  Without this,
// llvm-objdump prints the raw displacement and nothing else -- it takes the
// address from here to look the symbol up -- while binutils names the target
// on every branch.
namespace {
class AlphaMCInstrAnalysis : public MCInstrAnalysis {
public:
  explicit AlphaMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    if (Inst.getNumOperands() == 0 ||
        Info->get(Inst.getOpcode()).operands()[Inst.getNumOperands() - 1]
                .OperandType != MCOI::OPERAND_PCREL)
      return false;
    const MCOperand &Op = Inst.getOperand(Inst.getNumOperands() - 1);
    if (!Op.isImm())
      return false;
    // The displacement counts instructions from the one after the branch.
    Target = Addr + Size + Op.getImm() * 4;
    return true;
  }
};
} // end anonymous namespace

static MCInstrAnalysis *createAlphaMCInstrAnalysis(const MCInstrInfo *Info) {
  return new AlphaMCInstrAnalysis(Info);
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
  TargetRegistry::RegisterMCInstrAnalysis(T, createAlphaMCInstrAnalysis);
  TargetRegistry::RegisterMCRegInfo(T, createAlphaMCRegisterInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createAlphaMCSubtargetInfo);
  TargetRegistry::RegisterMCInstPrinter(T, createAlphaMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createAlphaMCCodeEmitter);
  TargetRegistry::RegisterMCAsmBackend(T, createAlphaAsmBackend);
}
