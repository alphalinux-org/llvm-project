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

  // -msmall-text emits a single bsr for a direct call instead of a GOT load and
  // jsr, assuming the whole program is in range and shares the global pointer.
  Handle(options::OPT_msmall_text, options::OPT_mlarge_text, "small-text");

  // -mno-fp-regs keeps the floating-point registers out of use entirely; the
  // kernel is built this way so it need not save FP state on kernel entry.
  Handle(options::OPT_mno_fp_regs, options::OPT_mfp_regs, "no-fp-regs");

  // -ffixed-$<n> reserves integer register $<n> from allocation.
  static const std::pair<unsigned, StringRef> FixedRegs[] = {
#define ALPHA_FIXED(N) {options::OPT_ffixed_alpha_##N, "reserve-r" #N}
      ALPHA_FIXED(0),  ALPHA_FIXED(1),  ALPHA_FIXED(2),  ALPHA_FIXED(3),
      ALPHA_FIXED(4),  ALPHA_FIXED(5),  ALPHA_FIXED(6),  ALPHA_FIXED(7),
      ALPHA_FIXED(8),  ALPHA_FIXED(9),  ALPHA_FIXED(10), ALPHA_FIXED(11),
      ALPHA_FIXED(12), ALPHA_FIXED(13), ALPHA_FIXED(14), ALPHA_FIXED(15),
      ALPHA_FIXED(16), ALPHA_FIXED(17), ALPHA_FIXED(18), ALPHA_FIXED(19),
      ALPHA_FIXED(20), ALPHA_FIXED(21), ALPHA_FIXED(22), ALPHA_FIXED(23),
      ALPHA_FIXED(24), ALPHA_FIXED(25), ALPHA_FIXED(26), ALPHA_FIXED(27),
      ALPHA_FIXED(28), ALPHA_FIXED(29), ALPHA_FIXED(30), ALPHA_FIXED(31),
#undef ALPHA_FIXED
  };
  for (const auto &[Opt, Feat] : FixedRegs)
    if (Args.hasArg(Opt))
      Features.push_back(Args.MakeArgString("+" + Feat));
}
