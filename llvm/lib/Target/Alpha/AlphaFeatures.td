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
def NotBWX : Predicate<"!Subtarget->hasBWX()">;

// Motion Video Instructions.  (GCC spells the enabling flag -mmax, but DEC's
// name for the instruction set is MVI.)
def FeatureMVI : SubtargetFeature<"mvi", "HasMVI", "true",
                                  "Enable motion video instructions (MVI)">;
def HasMVI : Predicate<"Subtarget->hasMVI()">,
             AssemblerPredicate<(all_of FeatureMVI)>;

// Without BWX, use a lock-based (ldq_l/stq_c) sequence for byte/word stores so
// that they are safe against concurrent accesses to the same quadword.
def FeatureSafeBWA : SubtargetFeature<"safe-bwa", "HasSafeBWA", "true",
    "Use a lock-based sequence for pre-BWX byte/word stores">;

// Address global data GP-relative (gprel) rather than through the GOT.  This is
// gcc's -msmall-data (the default is -mlarge-data, i.e. GOT/literal).
def FeatureSmallData : SubtargetFeature<"small-data", "HasSmallData", "true",
    "Address global data GP-relative instead of through the GOT">;
// Pre-BWX byte/word store variants: plain read-modify-write vs. lock-based.
def UnsafeBWStore
    : Predicate<"!Subtarget->hasBWX() && !Subtarget->hasSafeBWA()">;
def SafeBWStore
    : Predicate<"!Subtarget->hasBWX() && Subtarget->hasSafeBWA()">;

def FeatureFIX : SubtargetFeature<"fix", "HasFIX", "true",
                                  "Enable FIX floating-point conversion "
                                  "instructions (sqrt, cvtst)">;
def HasFIX : Predicate<"Subtarget->hasFIX()">,
             AssemblerPredicate<(all_of FeatureFIX)>;

def FeatureCIX : SubtargetFeature<"cix", "HasCIX", "true",
                                  "Enable count instructions (CIX: ctlz, "
                                  "cttz, ctpop)">;
def HasCIX : Predicate<"Subtarget->hasCIX()">,
             AssemblerPredicate<(all_of FeatureCIX)>;
