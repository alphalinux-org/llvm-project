//===-- AlphaLegalizerInfo.cpp --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the targeting of the MachineLegalizer class for Alpha.
//
//===----------------------------------------------------------------------===//

#include "AlphaLegalizerInfo.h"
#include "AlphaSubtarget.h"
#include "llvm/CodeGen/GlobalISel/LegalizerHelper.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/ValueTypes.h"

using namespace llvm;
using namespace LegalizeActions;

AlphaLegalizerInfo::AlphaLegalizerInfo(const AlphaSubtarget &ST) {
  using namespace TargetOpcode;
  const LLT s1 = LLT::scalar(1);
  const LLT s8 = LLT::scalar(8);
  const LLT s16 = LLT::scalar(16);
  const LLT s32 = LLT::scalar(32);
  const LLT s64 = LLT::scalar(64);
  const LLT p0 = LLT::pointer(0, 64);

  // Without floating-point registers there is nothing for a floating operation
  // to be selected into: register allocation fails with "no registers from
  // class available to allocate", and invariant B3 catches whatever reaches it
  // first.  The SelectionDAG path does not make the floating types legal at
  // all under this option and compiles such a function to the soft-float
  // libcalls, so every floating rule below is guarded to hand the function to
  // that path whole.  AlphaCallLowering does the same for a floating value in
  // the signature, which the legalizer never sees.
  const LegalityPredicate NoFPRegs =
      [HasNoFPRegs = ST.hasNoFPRegs()](const LegalityQuery &) {
        return HasNoFPRegs;
      };

  // A register is a quadword and the 32-bit operations sign-extend their
  // result, so a narrower integer is kept widened to one rather than given
  // operations of its own.
  getActionDefinitionsBuilder({G_ADD, G_SUB, G_MUL, G_AND, G_OR, G_XOR})
      .legalFor({s64})
      .widenScalarToNextPow2(0)
      .clampScalar(0, s64, s64);

  // There is no divide instruction.  The SelectionDAG path calls __divq and
  // friends, which take their arguments in $24/$25 and return in $27 rather
  // than in the usual registers, so the generic libcall machinery cannot be
  // pointed at them.  Hand such a function to that path whole.
  getActionDefinitionsBuilder({G_SDIV, G_UDIV, G_SREM, G_UREM}).unsupported();

  getActionDefinitionsBuilder({G_SHL, G_LSHR, G_ASHR})
      .legalFor({{s64, s64}})
      .clampScalar(0, s64, s64)
      .clampScalar(1, s64, s64);

  getActionDefinitionsBuilder(G_CONSTANT)
      .legalFor({s64, p0})
      .clampScalar(0, s64, s64);

  getActionDefinitionsBuilder({G_FRAME_INDEX, G_GLOBAL_VALUE}).legalFor({p0});

  getActionDefinitionsBuilder(G_PTR_ADD).legalFor({{p0, s64}});

  getActionDefinitionsBuilder({G_INTTOPTR, G_PTRTOINT})
      .legalFor({{p0, s64}, {s64, p0}});

  getActionDefinitionsBuilder({G_LOAD, G_STORE})
      // The fourth field is the alignment, in bits, at or above which the
      // access is legal, and it must be the natural one.  Naming 8 -- one byte
      // -- would declare a quadword load from a one-byte-aligned address legal,
      // because `isCompatible` accepts any alignment at or above what the rule
      // names.  Alpha has no such access: the datum can straddle two quadwords,
      // and the SelectionDAG path lowers it to the ldq_u/extql/extqh pair
      // `LowerLOAD` builds, or for a store to the `USTORE` pseudo and its
      // custom inserter.  GlobalISel runs neither.
      .legalForTypesWithMemDesc({{s64, p0, s8, 8},
                                 {s64, p0, s16, 16},
                                 {s64, p0, s32, 32},
                                 {s64, p0, s64, 64},
                                 {p0, p0, s64, 64}})
      // What is left is a misaligned access, and it goes to the SelectionDAG
      // path whole rather than to `.lower()`, which would split it into byte
      // accesses and is much worse than the two-instruction sequence that path
      // already emits.  This is the same choice, for the same reason, as the
      // atomics and the jump tables in this file: say `unsupported` and hand
      // the function over, rather than claim a legality the selector cannot
      // honour.
      .unsupportedIf([](const LegalityQuery &Q) {
        return !Q.MMODescrs.empty() &&
               Q.MMODescrs[0].AlignInBits <
                   Q.MMODescrs[0].MemoryTy.getSizeInBits();
      })
      .clampScalar(0, s64, s64)
      .lower();

  // The narrow destinations are here because maxScalar is a ceiling only: with
  // s64 destinations alone, an extension *to* s32 -- which is what a `sext i8
  // to i32` feeding a 32-bit store produces -- would match no entry and would
  // not be widened up either.  Pinning the destination at s64 with clampScalar
  // is not the answer: widenScalar has no case for widening the destination of
  // an extension, so the rule would ask for something the helper cannot do and
  // fail one step later.
  //
  // The narrow destination is legal as it stands.  A register is a quadword
  // whatever the type says, the selector switches on the *source* width alone,
  // and sign-extending a byte into a whole register leaves the low 32 bits
  // holding exactly the s32 value.
  getActionDefinitionsBuilder({G_SEXT, G_ZEXT, G_ANYEXT})
      .legalFor({{s64, s1}, {s64, s8}, {s64, s16}, {s64, s32},
                 {s32, s1}, {s32, s8}, {s32, s16}})
      .maxScalar(0, s64);

  // The extending loads need a rule of their own rather than no rule at all:
  // an opcode with no rules has an empty rule set that verify() accepts
  // vacuously, so leaving them out leaves nothing checking them.  They are
  // reachable -- lowering a non-power-of-two load (an i40 at -O0) produces a
  // G_ZEXTLOAD from an s32.
  //
  // The selector has no counterpart for either opcode.  `.lower()` looks like
  // the answer and is not: lowerLoad splits a load, and for a byte-sized,
  // power-of-two, naturally aligned extending load -- which is exactly this
  // one -- it has nothing to split and returns UnableToLegalize, so the rule
  // would fail one step further on.  Say unsupported and hand the function to
  // the SelectionDAG path, as for the atomics above.
  //
  // Selecting these directly is the improvement to make here: ldl already
  // sign-extends a longword, and ldbu/ldwu zero-extend a byte and a word
  // where there is BWX.  That is selector work, not a legalizer rule.
  getActionDefinitionsBuilder({G_ZEXTLOAD, G_SEXTLOAD}).unsupported();

  getActionDefinitionsBuilder(G_TRUNC).alwaysLegal();

  // A compare writes 0 or 1 into a general register; there is no condition
  // register and no i1, so the result type that describes the machine is s64.
  // Leaving {s1, s64} legal here would be accepted before clampScalar ever
  // runs, and every imported compare pattern is rooted at (setcc:{i64} ...),
  // so an s1 result puts all of them -- the whole cmp*i literal family
  // included -- out of reach of the selector, and forces a G_ZEXT s1->s64
  // whose `and $reg, 1, $reg' is dead by construction.
  getActionDefinitionsBuilder(G_ICMP)
      .legalFor({{s64, s64}, {s64, p0}})
      .clampScalar(0, s64, s64)
      .clampScalar(1, s64, s64);

  // A branch tests a whole register against zero, and the condition it is
  // given holds 0 or 1, so no narrowing is needed.
  getActionDefinitionsBuilder(G_BRCOND).legalFor({s64}).minScalar(0, s64);

  getActionDefinitionsBuilder(G_PHI).legalFor({s64, p0}).clampScalar(0, s64,
                                                                     s64);
  getActionDefinitionsBuilder(G_BR).alwaysLegal();

  getActionDefinitionsBuilder({G_FADD, G_FSUB, G_FMUL, G_FDIV})
      .unsupportedIf(NoFPRegs)
      .legalFor({s32, s64});

  getActionDefinitionsBuilder(G_FCONSTANT)
      .unsupportedIf(NoFPRegs)
      .legalFor({s32, s64});

  getActionDefinitionsBuilder({G_FPEXT, G_FPTRUNC})
      .unsupportedIf(NoFPRegs)
      .legalFor({{s64, s32}, {s32, s64}});

  // The memory intrinsics become libcalls, as they do on the SelectionDAG
  // path.
  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE, G_MEMSET}).libcall();
  getActionDefinitionsBuilder({G_MEMCPY_INLINE, G_MEMSET_INLINE}).lower();

  verify(*ST.getInstrInfo());
}
