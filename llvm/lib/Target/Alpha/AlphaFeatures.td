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

// -msafe-partial: a misaligned store spanning a 64-bit boundary updates each
// containing quadword with a read-modify-write; use a lock-based (ldq_l/stq_c)
// sequence so it is safe against concurrent access to adjacent bytes.
def FeatureSafePartial : SubtargetFeature<"safe-partial", "HasSafePartial",
    "true", "Use a lock-based sequence for partial/unaligned stores">;


// -msmall-text: the whole program's code fits in 4 MiB (reachable by a branch)
// and shares one global pointer, so a direct call is a single bsr and functions
// need not re-establish the gp on entry.
def FeatureSmallText : SubtargetFeature<"small-text", "HasSmallText", "true",
    "Assume all code is within branch range of a single global pointer">;

// Build every wide integer constant with code instead of loading it from the
// constant pool.  This is gcc's -mbuild-constants; the dynamic loader needs it
// because it must relocate itself before it can reach its own data segment.
def FeatureBuildConstants
    : SubtargetFeature<"build-constants", "HasBuildConstants", "true",
                       "Materialize wide constants inline instead of loading "
                       "them from the constant pool">;

// Gather small, locally-defined globals into .sdata/.sbss near the global
// pointer.  This is gcc's -msmall-data; the default is -mlarge-data.  It is a
// placement decision only -- addressing does not key on it.
def FeatureSmallData : SubtargetFeature<"small-data", "HasSmallData", "true",
    "Place small globals in .sdata and .sbss">;
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

// -mieee-conformant: mark the generated code as IEEE conformant with a
// `.eflag 48' in each function prologue, which is what tells the loader to
// enable software completion for the object.  gcc documents this as the
// option's only effect, and alpha.cc does exactly that and nothing else -- it
// implies no other option, and it is the user's job to also ask for
// -mtrap-precision=i and -mfp-trap-mode=su or sui.
def FeatureIEEEConformant
    : SubtargetFeature<"ieee-conformant", "HasIEEEConformant", "true",
                       "Mark the object as IEEE conformant (.eflag 48)">;

// -mtrap-precision=i (and -mieee-conformant): follow each trapping
// floating-point instruction with a trap barrier so an arithmetic trap is
// precise to the instruction that raised it.
def FeatureTrapPrecisionInsn
    : SubtargetFeature<"trap-precision-insn", "HasTrapPrecisionInsn", "true",
                       "Insert trap barriers for instruction-precise FP traps">;

// The 21264 (ev6) and later implement precise arithmetic exceptions in
// hardware, so trapb is a no-op there and need not be emitted.
def FeaturePreciseArithTraps
    : SubtargetFeature<"precise-arith-traps", "HasPreciseArithTraps", "true",
                       "Arithmetic traps are precise in hardware (21264+)">;

// Software-directed prefetch (a load to R31/F31) is architecturally defined
// only on the 21264 and later; on earlier processors such a load is an ordinary
// load that can fault.  All three of the hints the 21264 defines -- normal,
// modify intent and evict next -- arrive together with it (21264/EV68A HRM
// sections 2.6.1 to 2.6.3), so one feature covers them all.
def FeaturePrefetch
    : SubtargetFeature<"prefetch", "HasPrefetch", "true",
                       "Support software-directed prefetch (21264 and later)">;
def HasPrefetch : Predicate<"Subtarget->hasPrefetch()">;

// -mno-fp-regs: do not use the floating-point registers at all.  The kernel is
// built this way so it need not save and restore floating-point state on every
// entry into the kernel.
def FeatureNoFPRegs : SubtargetFeature<"no-fp-regs", "HasNoFPRegs", "true",
    "Do not use the floating-point registers (-mno-fp-regs)">;

// -ffixed-$<n>: reserve integer register $<n> so the register allocator will
// not use it.  The kernel dedicates $8 to the current-thread pointer this way.
foreach i = 0 ... 31 in
  def FeatureReserveR#i : SubtargetFeature<"reserve-r"#i,
      "ReserveRegister["#i#" + Alpha::R0]", "true",
      "Reserve integer register $"#i>;

// -mieee: floating point traps are completed in software.
def HasIEEE : Predicate<"Subtarget->hasIEEE()">;
def NotIEEE : Predicate<"!Subtarget->hasIEEE()">;
