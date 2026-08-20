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

Alpha::FPRoundMode llvm::getFPRoundMode(const MCSubtargetInfo &STI) {
  if (STI.hasFeature(Alpha::FeatureFPRoundChopped))
    return Alpha::FPRoundChopped;
  if (STI.hasFeature(Alpha::FeatureFPRoundMinus))
    return Alpha::FPRoundMinus;
  if (STI.hasFeature(Alpha::FeatureFPRoundDynamic))
    return Alpha::FPRoundDynamic;
  return Alpha::FPRoundNormal;
}

// V32 = (Hi << 16) + Lo with Lo sign-extended, so Hi lies in [-0x8000,
// 0x8000].  The +0x8000 case does not fit ldah's signed field and is emitted as
// two ldah of 0x4000.  A value of zero still emits `lda 0`: the sequence is
// required to write its destination, and every caller that cannot reach zero
// says so for itself.
void Alpha::buildConstant32Steps(int32_t V32,
                                 SmallVectorImpl<ConstantStep> &Steps) {
  int64_t Lo = static_cast<int16_t>(V32);
  int64_t Hi = (static_cast<int64_t>(V32) - Lo) >> 16;
  if (Hi != 0) {
    if (isInt<16>(Hi)) {
      Steps.push_back({Alpha::LDAH, Hi});
    } else {
      Steps.push_back({Alpha::LDAH, Hi / 2});
      Steps.push_back({Alpha::LDAH, Hi / 2});
    }
  }
  if (Lo != 0 || Hi == 0)
    Steps.push_back({Alpha::LDA, Lo});
}

void Alpha::buildConstantSteps(int64_t V,
                               SmallVectorImpl<ConstantStep> &Steps) {
  if (isInt<32>(V)) {
    buildConstant32Steps(static_cast<int32_t>(V), Steps);
    return;
  }
  // V = (Hi32 << 32) + Lo32: build the adjusted high half, shift it up, then
  // add the low half; the sign of Lo32 is already accounted for in Hi32.  The
  // subtraction is done unsigned because V - Lo32 overflows a signed 64-bit
  // value for a V near INT64_MAX whose low half is negative, and the wrapped
  // result is the one wanted.
  uint64_t UV = static_cast<uint64_t>(V);
  int32_t Lo32 = static_cast<int32_t>(UV);
  int32_t Hi32 =
      static_cast<int32_t>((UV - static_cast<uint64_t>(int64_t(Lo32))) >> 32);
  buildConstant32Steps(Hi32, Steps);
  Steps.push_back({Alpha::SLLi, 32});
  if (Lo32 != 0)
    buildConstant32Steps(Lo32, Steps);
}

// See the comment on FieldOps in AlphaMCTargetDesc.h.
Alpha::FieldOps Alpha::getFieldOps(unsigned Bytes) {
  switch (Bytes) {
  case 1:
    return {Alpha::EXTBL, 0, Alpha::INSBL, 0, Alpha::MSKBL, 0};
  case 2:
    return {Alpha::EXTWL, Alpha::EXTWH, Alpha::INSWL,
            Alpha::INSWH, Alpha::MSKWL, Alpha::MSKWH};
  case 4:
    return {Alpha::EXTLL, Alpha::EXTLH, Alpha::INSLL,
            Alpha::INSLH, Alpha::MSKLL, Alpha::MSKLH};
  case 8:
    return {Alpha::EXTQL, Alpha::EXTQH, Alpha::INSQL,
            Alpha::INSQH, Alpha::MSKQL, Alpha::MSKQH};
  }
  llvm_unreachable("no field instructions for this width");
}

// See the comment on DirectCallInfo in AlphaMCTargetDesc.h.
bool Alpha::getDirectCallInfo(unsigned Opc, Alpha::DirectCallInfo &Info) {
  struct Row {
    unsigned Opc;
    Alpha::DirectCallInfo Info;
  };
  static constexpr Row Rows[] = {
      {Alpha::JSRd, {3, false, true}},
      {Alpha::JSRdl, {3, false, false}},
      {Alpha::JSRtlsgd, {4, false, false}},
      {Alpha::JSRtlsldm, {5, false, false}},
      {Alpha::TCRETURNd, {3, true, true}},
      {Alpha::TCRETURNdl, {3, true, false}},
  };
  for (const Row &R : Rows)
    if (R.Opc == Opc) {
      Info = R.Info;
      return true;
    }
  return false;
}

// The names GNU as accepts, which are the R_ALPHA_LITUSE use types in
// include/elf/alpha.h.
const char *Alpha::getLituseName(unsigned Type) {
  switch (Type) {
  case 0:
    return "lituse_addr";
  case 1:
    return "lituse_base";
  case 2:
    return "lituse_bytoff";
  case 3:
    return "lituse_jsr";
  case 4:
    return "lituse_tlsgd";
  case 5:
    return "lituse_tlsldm";
  case 6:
    return "lituse_jsrdirect";
  }
  llvm_unreachable("no name for this lituse use type");
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
