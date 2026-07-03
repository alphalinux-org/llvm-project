//===- AlphaMachineFunctionInfo.h - Alpha machine function info -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHAMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_ALPHA_ALPHAMACHINEFUNCTIONINFO_H

#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class AlphaMachineFunctionInfo : public MachineFunctionInfo {
  /// Whether the function establishes and uses the global pointer ($gp),
  /// which requires an ldgp in the prologue.
  bool UsesGP = false;

  /// The virtual register holding the incoming hidden result pointer of a
  /// function returning in memory, which is returned again in $0.  0 if the
  /// function has no such argument.
  Register SRetReturnReg;

public:
  AlphaMachineFunctionInfo() = default;
  AlphaMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  bool usesGP() const { return UsesGP; }
  void setUsesGP(bool U = true) { UsesGP = U; }

  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register R) { SRetReturnReg = R; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAMACHINEFUNCTIONINFO_H
