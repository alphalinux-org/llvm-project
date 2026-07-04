//===--- Alpha.cpp - Alpha Helpers for Tools --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "clang/Options/Options.h"

using namespace clang::driver;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

void alpha::getAlphaTargetFeatures(const Driver &D, const ArgList &Args,
                                   std::vector<llvm::StringRef> &Features) {
  // -mbuild-constants builds wide integer constants with code rather than
  // loading them from the constant pool (needed for the dynamic loader).
  if (Arg *A = Args.getLastArg(options::OPT_mbuild_constants,
                               options::OPT_mno_build_constants)) {
    if (A->getOption().matches(options::OPT_mbuild_constants))
      Features.push_back("+build-constants");
    else
      Features.push_back("-build-constants");
  }
}
