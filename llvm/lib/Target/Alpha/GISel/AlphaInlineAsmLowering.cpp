//===-- AlphaInlineAsmLowering.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the lowering of LLVM inline asm to machine code
// INLINEASM for GlobalISel.
//
//===----------------------------------------------------------------------===//

#include "AlphaInlineAsmLowering.h"
#include "AlphaISelLowering.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

AlphaInlineAsmLowering::AlphaInlineAsmLowering(const AlphaTargetLowering *TLI)
    : InlineAsmLowering(TLI) {}

bool AlphaInlineAsmLowering::lowerAsmOperandForConstraint(
    Value *Val, StringRef Constraint, std::vector<MachineOperand> &Ops,
    MachineIRBuilder &MIRBuilder) const {
  // The constant-integer constraints (GCC's alpha letters).  The generic
  // lowering knows only the target-independent ones, so without this a "I" or
  // "K" operand sends the whole function to the SelectionDAG path.  The ranges
  // are the ones LowerAsmOperandForConstraint enforces on that path.
  if (Constraint.size() == 1 && StringRef("IJKLMNOPS").contains(Constraint[0])) {
    auto *CI = dyn_cast<ConstantInt>(Val);
    if (!CI || CI->getBitWidth() > 64)
      return false;
    if (!Alpha::isValidConstantConstraint(Constraint[0], CI->getSExtValue(),
                                          CI->getZExtValue()))
      return false;
    Ops.push_back(MachineOperand::CreateImm(CI->getSExtValue()));
    return true;
  }

  // "R" is the name of something a bsr can reach directly; the asm printer
  // prints the symbol rather than a register holding its address.
  if (Constraint == "R") {
    if (auto *GV = dyn_cast<GlobalValue>(Val)) {
      Ops.push_back(MachineOperand::CreateGA(GV, /*Offset=*/0));
      return true;
    }
    // The SelectionDAG path takes a block address here too.  There is no
    // Value for an external symbol at this level, so that case has no
    // counterpart.
    if (auto *BA = dyn_cast<BlockAddress>(Val)) {
      Ops.push_back(MachineOperand::CreateBA(BA, /*Offset=*/0));
      return true;
    }
    return false;
  }

  return InlineAsmLowering::lowerAsmOperandForConstraint(Val, Constraint, Ops,
                                                         MIRBuilder);
}
