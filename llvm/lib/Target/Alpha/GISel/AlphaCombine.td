//=- AlphaCombine.td - Define Alpha Combine Rules ------------*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/GlobalISel/Combine.td"

// Combines that run before legalization.  The memory intrinsics are the only
// thing here: the legalizer turns them into libcalls, so a small constant
// length has to be recognised while it is still an intrinsic.
def AlphaPreLegalizerCombiner
    : GICombiner<"AlphaPreLegalizerCombinerImpl", [memcpy_family_combines]> {
}

// Combines that run after legalization, and so have to leave the function
// legal.  Everything here is an optimization: the target is correct without
// them, just slower.
def AlphaPostLegalizerCombiner
    : GICombiner<"AlphaPostLegalizerCombinerImpl", [combines_for_extload]> {
}
