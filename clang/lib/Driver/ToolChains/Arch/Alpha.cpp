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

  // The instruction-set extensions a processor implies come from its
  // ProcessorModel in Alpha.td, which -mcpu selects via -target-cpu; there is
  // nothing to add here for them.

  // Explicit -m<ext>/-mno-<ext> flags, which override the processor defaults.
  auto Handle = [&](unsigned Pos, unsigned Neg, StringRef Feat) {
    if (Arg *A = Args.getLastArg(Pos, Neg))
      Features.push_back(A->getOption().matches(Pos)
                             ? Args.MakeArgString("+" + Feat)
                             : Args.MakeArgString("-" + Feat));
  };
  Handle(options::OPT_mbwx, options::OPT_mno_bwx, "bwx");
  Handle(options::OPT_mcix, options::OPT_mno_cix, "cix");
  Handle(options::OPT_mmax, options::OPT_mno_max, "mvi");
  Handle(options::OPT_mfix, options::OPT_mno_fix, "fix");

  // IEEE floating-point conformance.  -mieee-with-inexact implies -mieee.
  if (Args.hasArg(options::OPT_mieee_with_inexact)) {
    Features.push_back("+ieee-with-inexact");
    // -mieee-with-inexact implies -mieee; claim the weaker flag to silence
    // "unsupported option" when both are passed (as glibc does).
    Args.ClaimAllArgs(options::OPT_mieee);
    Args.ClaimAllArgs(options::OPT_mno_ieee);
  } else {
    Handle(options::OPT_mieee, options::OPT_mno_ieee, "ieee");
  }
}
