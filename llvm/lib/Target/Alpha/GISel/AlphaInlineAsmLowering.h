//===-- AlphaInlineAsmLowering.h --------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes how to lower LLVM inline asm to machine code INLINEASM
// for GlobalISel.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAINLINEASMLOWERING_H
#define LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAINLINEASMLOWERING_H

#include "llvm/CodeGen/GlobalISel/InlineAsmLowering.h"

namespace llvm {

class AlphaTargetLowering;

class AlphaInlineAsmLowering : public InlineAsmLowering {
public:
  AlphaInlineAsmLowering(const AlphaTargetLowering *TLI);

  bool lowerAsmOperandForConstraint(Value *Val, StringRef Constraint,
                                    std::vector<MachineOperand> &Ops,
                                    MachineIRBuilder &MIRBuilder) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAINLINEASMLOWERING_H
