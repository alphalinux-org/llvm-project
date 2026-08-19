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

  /// Frame index of the integer register save area (the va_list base) in a
  /// variadic function.
  int VarArgsFrameIndex = 0;

  /// The initial va_list offset: the number of bytes of named arguments.
  unsigned VarArgsOffset = 0;

  /// Frame index of the slot holding the caller's frame pointer ($15), used
  /// only when the function needs a frame pointer.  -1 if none.
  int FramePointerSaveIndex = -1;

public:
  AlphaMachineFunctionInfo() = default;
  AlphaMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  bool usesGP() const { return UsesGP; }
  void setUsesGP(bool U = true) { UsesGP = U; }

  Register getSRetReturnReg() const { return SRetReturnReg; }
  void setSRetReturnReg(Register R) { SRetReturnReg = R; }

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int FI) { VarArgsFrameIndex = FI; }

  unsigned getVarArgsOffset() const { return VarArgsOffset; }
  void setVarArgsOffset(unsigned O) { VarArgsOffset = O; }

  int getFramePointerSaveIndex() const { return FramePointerSaveIndex; }
  void setFramePointerSaveIndex(int FI) { FramePointerSaveIndex = FI; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAMACHINEFUNCTIONINFO_H
