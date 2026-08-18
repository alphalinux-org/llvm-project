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

  /// Frame index of the integer register save area (the va_list base) in a
  /// variadic function.
  int VarArgsFrameIndex = 0;

  /// The initial va_list offset: the number of bytes of named arguments.
  unsigned VarArgsOffset = 0;

public:
  AlphaMachineFunctionInfo() = default;
  AlphaMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  bool usesGP() const { return UsesGP; }
  void setUsesGP(bool U = true) { UsesGP = U; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }

  unsigned getVarArgsOffset() const { return VarArgsOffset; }
  void setVarArgsOffset(unsigned O) { VarArgsOffset = O; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAMACHINEFUNCTIONINFO_H
