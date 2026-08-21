//===-- AlphaTargetObjectFile.cpp - Alpha Object Info ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaTargetObjectFile.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

// gcc's -G: globals no larger than this go in the small-data sections.  Its
// default, 8, keeps only scalars and small aggregates gp-addressable.
static cl::opt<unsigned>
    SSThreshold("alpha-ssection-threshold", cl::Hidden,
                cl::desc("Small data and BSS section maximum size (default 8)"),
                cl::init(8));

void AlphaTargetObjectFile::Initialize(MCContext &Ctx,
                                       const TargetMachine &TM) {
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
  SmallDataSection = getContext().getELFSection(
      ".sdata", ELF::SHT_PROGBITS, ELF::SHF_WRITE | ELF::SHF_ALLOC);
  SmallBSSSection = getContext().getELFSection(".sbss", ELF::SHT_NOBITS,
                                               ELF::SHF_WRITE | ELF::SHF_ALLOC);
}

bool AlphaTargetObjectFile::isGlobalInSmallSection(
    const GlobalObject *GO, const TargetMachine &TM) const {
  if (!TM.getMCSubtargetInfo().hasFeature(Alpha::FeatureSmallData))
    return false;

  // Only global variables, and only ones defined here: a gp-relative reference
  // needs the object's own offset from the global pointer, so external
  // declarations (which some other, possibly non-small-data, module defines)
  // stay in the GOT.
  const auto *GV = dyn_cast<GlobalVariable>(GO);
  if (!GV || GV->isDeclaration() || GV->hasCommonLinkage() ||
      GV->isThreadLocal())
    return false;

  Type *Ty = GV->getValueType();
  if (!Ty->isSized())
    return false;
  return GO->getDataLayout().getTypeAllocSize(Ty) <= SSThreshold;
}

MCSection *AlphaTargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  // A mergeable constant -- a string literal or a pool of equal-sized constants
  // -- keeps its mergeable section rather than moving to .sdata, which is
  // neither mergeable nor read-only.  gcc excludes string literals here too
  // (alpha_in_small_data_p bails on STRING_CST) and reaches the same
  // .rodata.str1.1; for the constant pools it does not, but putting a constant
  // in a writable section to save nothing is not worth matching -- addressing
  // is unaffected either way, since a .rodata object is still reached with
  // !gprelhigh/!gprellow.  A plain const scalar does go to .sdata, exactly as
  // gcc does: Alpha has no small read-only section, so
  // categorize_decl_for_section falls through to SECCAT_SDATA for it.
  if (!Kind.isMergeableCString() && !Kind.isMergeableConst() &&
      isGlobalInSmallSection(GO, TM))
    return Kind.isBSS() ? SmallBSSSection : SmallDataSection;
  return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
}
