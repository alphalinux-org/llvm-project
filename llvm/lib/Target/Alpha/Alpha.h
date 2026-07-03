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

FunctionPass *createAlphaISelDag(AlphaTargetMachine &TM,
                                 CodeGenOptLevel OptLevel);

} // namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHA_H
