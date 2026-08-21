//===-- AlphaTargetObjectFile.h - Alpha Object Info -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHATARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_ALPHA_ALPHATARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

/// With -msmall-data, small global objects are gathered into .sdata/.sbss so
/// the linker can place them near the global pointer, where they are reached by
/// a gp-relative address rather than a load from the GOT.
class AlphaTargetObjectFile : public TargetLoweringObjectFileELF {
  MCSection *SmallDataSection = nullptr;
  MCSection *SmallBSSSection = nullptr;

public:
  void Initialize(MCContext &Ctx, const TargetMachine &TM) override;

  /// True if GO is eligible to live in .sdata/.sbss (small-data enabled, a
  /// sized global variable no larger than the threshold, with a definition
  /// here so its gp-relative offset is known).
  bool isGlobalInSmallSection(const GlobalObject *GO,
                              const TargetMachine &TM) const;

  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHATARGETOBJECTFILE_H
