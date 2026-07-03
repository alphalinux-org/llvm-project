//===-- AlphaTargetInfo.cpp - Alpha Target Implementation -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheAlphaTarget() {
  static Target TheAlphaTarget;
  return TheAlphaTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaTargetInfo() {
  RegisterTarget<Triple::alpha, /*HasJIT=*/false> X(
      getTheAlphaTarget(), "alpha", "DEC Alpha", "Alpha");
}
