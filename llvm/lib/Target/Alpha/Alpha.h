//===-- Alpha.h - Top-level interface for Alpha representation --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the
// LLVM Alpha back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHA_H
#define LLVM_LIB_TARGET_ALPHA_ALPHA_H

#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/Support/CodeGen.h"

namespace llvm {

class AlphaTargetMachine;
class FunctionPass;

class AlphaRegisterBankInfo;
class AlphaSubtarget;
class GlobalValue;
class InstructionSelector;
class PassRegistry;

/// Whether a global's address can be computed from the global pointer instead
/// of being loaded from the GOT.
bool isAlphaGprelAddressable(const GlobalValue &GV);

FunctionPass *createAlphaTrapBarriers();
void initializeAlphaTrapBarriersPass(PassRegistry &);
FunctionPass *createAlphaExpandAtomicPseudo();
void initializeAlphaExpandAtomicPseudoPass(PassRegistry &);
FunctionPass *createAlphaVerifyInvariants();
void initializeAlphaVerifyInvariantsPass(PassRegistry &);
InstructionSelector *
createAlphaInstructionSelector(const AlphaTargetMachine &TM,
                               const AlphaSubtarget &STI,
                               const AlphaRegisterBankInfo &RBI);
FunctionPass *createAlphaISelDag(AlphaTargetMachine &TM,
                                 CodeGenOptLevel OptLevel);

} // namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHA_H
