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
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaTarget() {
  RegisterTargetMachine<AlphaTargetMachine> X(getTheAlphaTarget());
  PassRegistry &PR = *PassRegistry::getPassRegistry();
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
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()) {
  // Reconcile call-frame information across blocks so unwinding is correct at
  // any point, including within epilogues (see resetCFIToInitialState).
  setCFIFixup(true);
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
  void addPreEmitPass2() override;
};
} // end anonymous namespace

void AlphaPassConfig::addIRPasses() {
  // Expand atomic operations (inserting the memory barriers this target uses).
  addPass(createAtomicExpandLegacyPass());
  TargetPassConfig::addIRPasses();
}

void AlphaPassConfig::addPreEmitPass2() {
  // Build the ldq_l/stq_c loops, as late as the pipeline allows.  The
  // architecture requires that no memory access appear between the load locked
  // and the store conditional, and a spill, a reload or a reordering placed
  // there makes the store conditional fail every time round the loop, so the
  // expansion has to come after register allocation and the post-RA scheduler
  // -- and after anything later that could reintroduce one.
  addPass(createAlphaExpandAtomicPseudo());

  // Last of all, and only when asked for: check the property no test can
  // observe -- the reservation window this pass just built.  It has to follow
  // the expansion, because before it the window is a single pseudo and there
  // is nothing to look inside.
  addPass(createAlphaVerifyInvariants());
}

bool AlphaPassConfig::addInstSelector() {
  addPass(createAlphaISelDag(getAlphaTargetMachine(), getOptLevel()));
  return false;
}

TargetPassConfig *AlphaTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new AlphaPassConfig(*this, PM);
}
