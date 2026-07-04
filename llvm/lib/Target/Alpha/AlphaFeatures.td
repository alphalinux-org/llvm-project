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

// Build every wide integer constant with code instead of loading it from the
// constant pool.  This is gcc's -mbuild-constants; the dynamic loader needs it
// because it must relocate itself before it can reach its own data segment.
def FeatureBuildConstants
    : SubtargetFeature<"build-constants", "HasBuildConstants", "true",
                       "Materialize wide constants inline instead of loading "
                       "them from the constant pool">;
// Pre-BWX byte/word store variants: plain read-modify-write vs. lock-based.
def UnsafeBWStore
    : Predicate<"!Subtarget->hasBWX() && !Subtarget->hasSafeBWA()">;
def SafeBWStore
    : Predicate<"!Subtarget->hasBWX() && Subtarget->hasSafeBWA()">;

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

// IEEE floating-point conformance (-mieee): arithmetic gets the software-
// completion trap qualifier (/su) so denormals and exceptional values are
// handled correctly.  -mieee-with-inexact adds inexact tracking (/sui).
def FeatureIEEE : SubtargetFeature<"ieee", "HasIEEE", "true",
                                   "Generate IEEE-conformant floating point "
                                   "with software completion (-mieee)">;
def FeatureIEEEInexact
    : SubtargetFeature<"ieee-with-inexact", "HasIEEEInexact", "true",
                       "IEEE floating point that also tracks the inexact flag "
                       "(-mieee-with-inexact)", [FeatureIEEE]>;

// -mfp-trap-mode=u: trap on underflow but without software completion (the /u
// qualifier, as opposed to /su under -mieee).
def FeatureFPTrapU : SubtargetFeature<"fptrap-u", "HasFPTrapU", "true",
                                      "Trap on floating-point underflow (/u)">;

// -mfp-rounding-mode: the IEEE rounding applied to floating-point operate
// instructions.  Normal (round to nearest) is the default and needs no feature.
def FeatureFPRoundDynamic
    : SubtargetFeature<"fpround-dynamic", "HasFPRoundDynamic", "true",
                       "Round floating point per the FPCR at run time (/d)">;
def FeatureFPRoundMinus
    : SubtargetFeature<"fpround-minus", "HasFPRoundMinus", "true",
                       "Round floating point toward minus infinity (/m)">;
def FeatureFPRoundChopped
    : SubtargetFeature<"fpround-chopped", "HasFPRoundChopped", "true",
                       "Round floating point toward zero (/c)">;

// Software-directed prefetch (a load to R31/F31) is architecturally defined
// only on the 21264 and later; on earlier processors such a load is an ordinary
// load that can fault.  The evict-next hint (ldq/ldt to R31/F31) is defined only
// on the 21364 and later; elsewhere those localities use a normal prefetch.
def FeaturePrefetch
    : SubtargetFeature<"prefetch", "HasPrefetch", "true",
                       "Support software-directed prefetch (21264 and later)">;
def HasPrefetch : Predicate<"Subtarget->hasPrefetch()">;

def FeaturePrefetchEvictNext
    : SubtargetFeature<"prefetch-evict-next", "HasPrefetchEvictNext", "true",
                       "Support the evict-next prefetch hint (21364 and later)",
                       [FeaturePrefetch]>;
def HasPrefetchEvictNext : Predicate<"Subtarget->hasPrefetchEvictNext()">;

// -mieee: floating point traps are completed in software.
def HasIEEE : Predicate<"Subtarget->hasIEEE()">;
def NotIEEE : Predicate<"!Subtarget->hasIEEE()">;
