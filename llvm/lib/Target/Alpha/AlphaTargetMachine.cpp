//===-- AlphaTargetMachine.cpp - Define TargetMachine for Alpha -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaTargetMachine.h"
#include "Alpha.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaTargetObjectFile.h"
#include "AlphaTargetTransformInfo.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaTarget() {
  RegisterTargetMachine<AlphaTargetMachine> X(getTheAlphaTarget());
  // The GlobalISel passes this target's pass config adds have to be registered
  // before anything can require them.
  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeGlobalISel(PR);
  initializeAlphaTrapBarriersPass(PR);
  initializeAlphaExpandAtomicPseudoPass(PR);
  initializeAlphaVerifyInvariantsPass(PR);
}

static Reloc::Model getEffectiveRelocModel(bool JIT,
                                           std::optional<Reloc::Model> RM) {
  if (!RM || JIT)
    return Reloc::Static;
  return *RM;
}

AlphaTargetMachine::AlphaTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS, Options,
                               getEffectiveRelocModel(JIT, RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<AlphaTargetObjectFile>()) {
  // Reconcile call-frame information across blocks so unwinding is correct at
  // any point, including within epilogues (see resetCFIToInitialState).
  setCFIFixup(true);
  // Outline repeated instruction sequences from minsize functions.
  // TargetPassConfig adds the pass only when Options.EnableMachineOutliner is
  // set, and nothing sets it for Alpha: clang's -moutline is wired up per
  // target and llc has no flag for it at all, so without this the pass can
  // never run.  setSupportsDefaultOutlining then confines it to the functions
  // shouldOutlineFromFunctionByDefault permits, which is the minsize ones.
  this->Options.EnableMachineOutliner = true;
  setSupportsDefaultOutlining(true);
  // GlobalISel does not cover everything the SelectionDAG path does: the
  // legalizer marks a jump table, a va_arg, an alloca, an atomic and the rest
  // of the gaps unsupported so such a function is handed back to that path.
  // That is a fall back rather than an error only if abort is disabled, and it
  // is what every one of those cases is documented to do.
  setGlobalISelAbort(GlobalISelAbortMode::DisableWithDiag);
  initAsmInfo();
}

MachineFunctionInfo *AlphaTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return AlphaMachineFunctionInfo::create<AlphaMachineFunctionInfo>(Allocator,
                                                                    F, STI);
}

const AlphaSubtarget *
AlphaTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  auto &I = SubtargetMap[CPU + TuneCPU + FS];
  if (!I)
    I = std::make_unique<AlphaSubtarget>(TargetTriple, CPU, TuneCPU, FS, *this);
  return I.get();
}

TargetTransformInfo
AlphaTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<AlphaTTIImpl>(this, F));
}

namespace {
class AlphaPassConfig : public TargetPassConfig {
public:
  AlphaPassConfig(AlphaTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  AlphaTargetMachine &getAlphaTargetMachine() const {
    return getTM<AlphaTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  bool addIRTranslator() override;
  bool addLegalizeMachineIR() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  bool addILPOpts() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
};
} // end anonymous namespace

void AlphaPassConfig::addIRPasses() {
  // Expand atomic operations (inserting the memory barriers this target uses).
  addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}

bool AlphaPassConfig::addILPOpts() {
  // Reassociate operation chains to shorten the critical path.
  addPass(&MachineCombinerID);
  return true;
}

void AlphaPassConfig::addPreEmitPass() {
  // Under -mtrap-precision=i, follow each trapping FP instruction with a trapb.
  addPass(createAlphaTrapBarriers());
  // Rewrite branches whose target is beyond the 21-bit displacement.
  addPass(&BranchRelaxationPassID);
}

void AlphaPassConfig::addPreEmitPass2() {
  // Build the ldq_l/stq_c loops, as late as the pipeline allows.  The
  // architecture requires that no memory access appear between the load locked
  // and the store conditional, and a spill, a reload or a reordering placed
  // there makes the store conditional fail every time round the loop, so the
  // expansion has to come after register allocation and the post-RA scheduler
  // -- and after anything later that could reintroduce one.  The machine
  // outliner is such a pass: it runs between addPreEmitPass and
  // addPreEmitPass2, and expanding before it would let it treat the loop as
  // ordinary instructions and outline through it, putting a bsr and its return
  // jump inside the reservation window, which the architecture allows to clear
  // the lock flag.
  addPass(createAlphaExpandAtomicPseudo());

  // Last of all, and only when asked for: check the properties no test can
  // observe -- the reservation window this pass just built, -mno-fp-regs, and
  // the global pointer.  It has to follow the expansion, because before it the
  // window is a single pseudo and there is nothing to look inside.
  addPass(createAlphaVerifyInvariants());
}

bool AlphaPassConfig::addInstSelector() {
  addPass(createAlphaISelDag(getAlphaTargetMachine(), getOptLevel()));
  return false;
}

bool AlphaPassConfig::addIRTranslator() {
  addPass(new IRTranslator(getOptLevel()));
  return false;
}

bool AlphaPassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

bool AlphaPassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

bool AlphaPassConfig::addGlobalInstructionSelect() {
  addPass(new InstructionSelect(getOptLevel()));
  return false;
}

TargetPassConfig *AlphaTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new AlphaPassConfig(*this, PM);
}
