//===-- AlphaFeatures.td - Alpha subtarget features --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Alpha ISA extensions, matching GCC's -m{bwx,max,fix,cix} options.
//
//===----------------------------------------------------------------------===//

def FeatureBWX : SubtargetFeature<"bwx", "HasBWX", "true",
                                  "Enable byte/word memory instructions (BWX)">;
def HasBWX : Predicate<"Subtarget->hasBWX()">,
             AssemblerPredicate<(all_of FeatureBWX)>;

// Motion Video Instructions.  (GCC spells the enabling flag -mmax, but DEC's
// name for the instruction set is MVI.)
def FeatureMVI : SubtargetFeature<"mvi", "HasMVI", "true",
                                  "Enable motion video instructions (MVI)">;
def HasMVI : Predicate<"Subtarget->hasMVI()">,
             AssemblerPredicate<(all_of FeatureMVI)>;

def FeatureFIX : SubtargetFeature<"fix", "HasFIX", "true",
                                  "Enable FIX square-root and register-move "
                                  "instructions (sqrts, sqrtt, itof, ftoi)">;
def HasFIX : Predicate<"Subtarget->hasFIX()">,
             AssemblerPredicate<(all_of FeatureFIX)>;

def FeatureCIX : SubtargetFeature<"cix", "HasCIX", "true",
                                  "Enable count instructions (CIX: ctlz, "
                                  "cttz, ctpop)">;
def HasCIX : Predicate<"Subtarget->hasCIX()">,
             AssemblerPredicate<(all_of FeatureCIX)>;
