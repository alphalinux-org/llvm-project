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

  // A shift of a scalar wider than a quadword is split into quadwords, which
  // works only when the width is a whole number of them.  clampScalar splits
  // in half and half of an s120 is an s60, whose own legalization goes looking
  // for the pieces 60 and 8 have in common: legalizing one such shift spends
  // millions of virtual registers unmerging s60s into s4s and does not finish.
  // Two csmith programs in a forty-seed census stopped compiling on this once
  // the loads above them became legal enough to reach it.  Hand such a
  // function to the SelectionDAG path, which expands the shift itself.
  //
  // Rounding the width up to the next power of two first is the obvious
  // alternative and it is not one: an s65 shift widened to s128 makes the
  // legalizer and the artifact combiner pass an unmerge of a quadword into 64
  // booleans back and forth forever.  The one that has been measured to
  // terminate is the one that refuses.
  getActionDefinitionsBuilder({G_SHL, G_LSHR, G_ASHR})
      .legalFor({{s64, s64}})
      .unsupportedIf([=](const LegalityQuery &Query) {
        const unsigned Bits = Query.Types[0].getSizeInBits();
        return Bits > 64 && Bits % 64 != 0;
      })
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
      // The s32 entry is what lets a float stay in the floating-point bank: a
      // register is a quadword, so an integer load widens to s64, but lds and
      // sts move a 32-bit float directly and convert the S_floating format on
      // the way.  Without it every float load widened to s64, landed in a
      // general register and had to reach $f0 through the stack.
      //
      // The fourth field is the alignment, in bits, at or above which the
      // access is legal, and every entry names one byte.  `isCompatible`
      // accepts any alignment at or above what the rule names, so this declares
      // the misaligned forms legal too -- deliberately.  Alpha has no
      // misaligned access, and the datum can straddle two quadwords, but that
      // is a matter for the selector, which expands one to the same
      // ldq_u/extql/extqh sequence `AlphaTargetLowering::LowerLOAD` builds, or
      // for a store to the read-modify-write pseudo `LowerSTORE` builds.  The
      // alternative, saying `unsupported` and handing the function to the
      // SelectionDAG path, cost the fallback more than half of every function
      // this target failed to build under `-global-isel-abort=1`: a load from a
      // packed struct member is enough to trigger it.
      //
      // Two, four and eight bytes are what the extract instructions cover.  A
      // width that is none of those cannot arrive with a natural alignment to
      // fall short of -- it is not a power of two -- so it never matches here,
      // and `lower()` below splits it into accesses that do.
      .legalForTypesWithMemDesc({{s64, p0, s8, 8},
                                 {s64, p0, s16, 8},
                                 {s64, p0, s32, 8},
                                 {s64, p0, s64, 8},
                                 {s32, p0, s32, 8},
                                 {p0, p0, s64, 8}})
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
  //
  // The source width is not a list either, because it is not a list of the
  // widths a register can hold: lowering a load of an odd size produces an
  // extension from that size, and a three-byte load at -O2 is enough to do it.
  // Neither widening nor narrowing nor lowering can help there --
  // LegalizerHelper has no widenScalar case for an extension at all, and
  // lowerEXT handles only vectors -- so the width has to be legal here or the
  // function falls back. The selector extends any of them: a whole number of
  // bytes is a zapnot mask, and anything else is a pair of shifts, which is
  // what a byte and a word cost in any case.
  getActionDefinitionsBuilder({G_SEXT, G_ZEXT, G_ANYEXT})
      .legalIf([=](const LegalityQuery &Query) {
        const LLT Dst = Query.Types[0];
        const LLT Src = Query.Types[1];
        return Dst.isScalar() && Src.isScalar() && (Dst == s32 || Dst == s64) &&
               Src.getSizeInBits() < Dst.getSizeInBits();
      })
      .maxScalar(0, s64);

  // The extending loads need a rule of their own rather than no rule at all:
  // an opcode with no rules has an empty rule set that verify() accepts
  // vacuously, so leaving them out leaves nothing checking them.  They are
  // reachable -- lowering a non-power-of-two load (an i40 at -O0) produces a
  // G_ZEXTLOAD from an s32.
  //
  // They are selected directly, because the load that reads the bytes already
  // fills the register above them one way or the other: ldl sign-extends a
  // longword, ldbu and ldwu zero-extend a byte and a word where there is BWX,
  // and both the pre-BWX extract and the misaligned one zero-extend whatever
  // they extract.  So one of the two extensions costs nothing and the other
  // costs one instruction, which is what the SelectionDAG path pays too.
  //
  // Each entry names one byte as its alignment, for the reason the load rule
  // above gives: the misaligned form of each is selected as well.
  //
  // `.lower()` is what is left, and it only ever *splits* a load; for a
  // byte-sized, power-of-two one -- which is what an entry here would be -- it
  // has nothing to split and returns UnableToLegalize.  It is reached only by
  // the widths that are not powers of two, which is exactly what it can do.
  getActionDefinitionsBuilder({G_ZEXTLOAD, G_SEXTLOAD})
      .legalForTypesWithMemDesc(
          {{s64, p0, s8, 8}, {s64, p0, s16, 8}, {s64, p0, s32, 8}})
      .clampScalar(0, s64, s64)
      .lower();

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

  // A floating compare leaves its answer in an integer register, the way an
  // integer compare does.
  getActionDefinitionsBuilder(G_FCMP)
      .unsupportedIf(NoFPRegs)
      .legalFor({{s64, s32}, {s64, s64}})
      .clampScalar(0, s64, s64);

  getActionDefinitionsBuilder(G_FCONSTANT)
      .unsupportedIf(NoFPRegs)
      .legalFor({s32, s64});

  getActionDefinitionsBuilder({G_FPEXT, G_FPTRUNC})
      .unsupportedIf(NoFPRegs)
      .legalFor({{s64, s32}, {s32, s64}});

  // A conversion goes through a whole register: the value is moved between the
  // banks and converted there, so the integer side is always 64 bits.
  getActionDefinitionsBuilder(G_SITOFP)
      .unsupportedIf(NoFPRegs)
      .legalFor({{s32, s64}, {s64, s64}})
      .clampScalar(1, s64, s64);

  getActionDefinitionsBuilder(G_FPTOSI)
      .unsupportedIf(NoFPRegs)
      .legalFor({{s64, s32}, {s64, s64}})
      .clampScalar(0, s64, s64);

  // A conditional move leaves its destination alone when the condition is zero,
  // so the false value is what the destination already holds.
  // A 32-bit choice is legal as it stands: cmovne and fcmovne both move whole
  // registers and care nothing for the width of what is in them.  Widening it
  // would put a float through a pair of integer registers and cost a round trip
  // through memory at each end.
  //
  // clampScalar moves a type outside the range to the nearest end and leaves
  // one inside it alone, so an s40 select -- inside [s32, s64] and legal for
  // neither -- would pass straight through to a selector with nothing to
  // select.  Rounding up to the next power of two afterwards sends it to s64.
  //
  // The condition is a whole quadword and nothing narrower: there is no i1 on
  // this machine, so a boolean condition is widened by the clamp below rather
  // than made legal in its own right.  There would be no narrowing to be had
  // even if it were -- LegalizerHelper::narrowScalarSelect refuses every
  // operand but the destination -- so a rule that clamped the condition back
  // down to s1 would report legal here and fail one step later.
  //
  // Nothing in the selector has to know about this.  cmovne's own pattern is
  // written on an i64 condition, so the condition as it stands is what the
  // imported patterns match; selectSelect exists for the floating bank, where
  // there is no pattern to match.  A widened condition is zero-extended and
  // not any-extended -- the target's boolean contents are ZeroOrOne -- so
  // testing the whole register is testing the truth value, which is the same
  // thing the SelectionDAG path relies on.
  getActionDefinitionsBuilder(G_SELECT)
      .legalFor({{s32, s64}, {s64, s64}, {p0, s64}})
      .clampScalar(0, s32, s64)
      .widenScalarToNextPow2(0, 32)
      .clampScalar(1, s64, s64);

  // There is no unsigned conversion instruction; both directions are built out
  // of the signed one.
  getActionDefinitionsBuilder(G_UITOFP)
      .unsupportedIf(NoFPRegs)
      .clampScalar(1, s64, s64)
      .lower();

  getActionDefinitionsBuilder(G_FPTOUI)
      .unsupportedIf(NoFPRegs)
      .clampScalar(0, s64, s64)
      .lower();

  // The memory intrinsics become libcalls, as they do on the SelectionDAG
  // path.
  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE, G_MEMSET}).libcall();
  getActionDefinitionsBuilder({G_MEMCPY_INLINE, G_MEMSET_INLINE}).lower();

  // A value with no defining computation, and the barrier that pins one down.
  getActionDefinitionsBuilder(
      {G_IMPLICIT_DEF, G_FREEZE, G_CONSTANT_FOLD_BARRIER})
      .legalFor({s32, s64, p0})
      .widenScalarToNextPow2(0)
      .clampScalar(0, s32, s64);

  // Moving a value between the banks is a real instruction under FIX and a
  // trip through the stack otherwise, which the DAG path also does.
  // Only the 64-bit pair has a bitconvert pattern.  The 32-bit one reaches
  // MOVi2f_S/MOVf2i_S through custom lowering and target nodes GlobalISel does
  // not produce, so it is left to the SelectionDAG path rather than called
  // legal for a selector that has no case for it.
  getActionDefinitionsBuilder(G_BITCAST).legalFor({{s64, s64}}).unsupported();

  // Alpha is scalar-only, so these only ever split or join a wide integer.
  for (unsigned Op : {G_MERGE_VALUES, G_UNMERGE_VALUES}) {
    unsigned BigTy = Op == G_MERGE_VALUES ? 0 : 1;
    unsigned LitTy = Op == G_MERGE_VALUES ? 1 : 0;
    getActionDefinitionsBuilder(Op)
        .widenScalarToNextPow2(LitTy, 8)
        .widenScalarToNextPow2(BigTy, 32)
        .clampScalar(LitTy, s8, s64)
        .clampScalar(BigTy, s16, LLT::scalar(128))
        .lower();
  }
  getActionDefinitionsBuilder({G_EXTRACT, G_INSERT}).lower();

  // G_SEXT_INREG is deliberately left without a rule.  Nothing produces it for
  // this target today, and giving it one enables a combine that rewrites the
  // narrowing of a boolean into it: that lowers to an sll/sra pair, where the
  // selector's own handling and the SelectionDAG path both produce a mask and
  // a negate.  Both are two instructions and both correct, so adding the rule
  // buys nothing and costs agreement between the two paths, which is the
  // property worth having.  Give it one together with a custom lowering that
  // keeps the existing sequence, if something ever needs it.

  // cpys and cpysn set the sign bit of a copy, so these are single
  // instructions rather than the mask-and-or lowering would give.
  getActionDefinitionsBuilder({G_FNEG, G_FABS})
      .unsupportedIf(NoFPRegs)
      .legalFor({s32, s64});
  // cpys takes the sign from one register and the rest from another, so both
  // operands are named.
  getActionDefinitionsBuilder(G_FCOPYSIGN)
      .unsupportedIf(NoFPRegs)
      .legalFor({{s32, s32}, {s64, s64}})
      .lower();

  // sqrts and sqrtt are FIX instructions; everything else is a libcall.
  getActionDefinitionsBuilder(G_FSQRT)
      .unsupportedIf(NoFPRegs)
      .legalFor(ST.hasFIX(), {s32, s64})
      .libcall();
  getActionDefinitionsBuilder(
      {G_FMA, G_FREM, G_FPOW, G_FEXP, G_FEXP2, G_FLOG, G_FLOG2, G_FLOG10,
       G_FSIN, G_FCOS, G_FTAN, G_FCEIL, G_FFLOOR, G_FRINT, G_FNEARBYINT,
       G_INTRINSIC_TRUNC, G_INTRINSIC_ROUND, G_INTRINSIC_ROUNDEVEN})
      .unsupportedIf(NoFPRegs)
      .libcall();
  getActionDefinitionsBuilder({G_FMINNUM, G_FMAXNUM})
      .unsupportedIf(NoFPRegs)
      .libcall();

  // ctpop, ctlz and cttz are CIX instructions.  Without them the generic
  // expansions apply, as they do on the SelectionDAG path.
  getActionDefinitionsBuilder({G_CTLZ, G_CTTZ, G_CTPOP})
      .legalFor(ST.hasCIX(), {{s64, s64}})
      .clampScalar(0, s64, s64)
      .clampScalar(1, s64, s64)
      .lower();
  getActionDefinitionsBuilder({G_CTLZ_ZERO_POISON, G_CTTZ_ZERO_POISON}).lower();

  // No byte-swap or absolute-value instruction, and the min/max forms are MVI
  // and operate on packed bytes rather than on a whole register, so these are
  // all built out of a compare and a conditional move.
  getActionDefinitionsBuilder(G_BSWAP).lower();
  getActionDefinitionsBuilder(G_ABS).minScalar(0, s64).lower();
  getActionDefinitionsBuilder({G_SMIN, G_SMAX, G_UMIN, G_UMAX})
      .minScalar(0, s64)
      .lower();
  getActionDefinitionsBuilder({G_SDIVREM, G_UDIVREM}).lower();
  getActionDefinitionsBuilder({G_SCMP, G_UCMP}).lower();
  getActionDefinitionsBuilder(
      {G_UADDO, G_USUBO, G_UADDE, G_USUBE, G_SADDO, G_SSUBO, G_SADDE, G_SSUBE})
      .lower();
  getActionDefinitionsBuilder(
      {G_SADDSAT, G_SSUBSAT, G_UADDSAT, G_USUBSAT, G_SSHLSAT, G_USHLSAT})
      .lower();
  getActionDefinitionsBuilder({G_FSHL, G_FSHR, G_ROTL, G_ROTR}).lower();
  // umulh is an instruction; there is no signed counterpart.  The SelectionDAG
  // path builds one out of umulh and two corrections, which the selector has no
  // case for, so a signed high multiply goes to that path instead.
  getActionDefinitionsBuilder(G_UMULH).legalFor({s64}).clampScalar(0, s64, s64);
  getActionDefinitionsBuilder(G_SMULH).unsupported();
  getActionDefinitionsBuilder({G_SMULO, G_UMULO}).lower();
  getActionDefinitionsBuilder(G_BITREVERSE).lower();

  // The address of a block and of a constant-pool entry are formed the same
  // gp-relative way a jump table's is, and by the same code.  The only thing
  // that reaches G_BRINDIRECT is an indirectbr, whose target is a block
  // address, so the three arrive together or not at all.
  getActionDefinitionsBuilder({G_BLOCK_ADDR, G_CONSTANT_POOL}).legalFor({p0});
  getActionDefinitionsBuilder(G_BRINDIRECT).legalFor({p0});

  // A jump table dispatch: the table address is formed gp-relative and the
  // dispatch reads a 32-bit gp-relative offset out of it, which is what
  // selectBrJT builds.  The index arrives as a scalar of pointer width --
  // emitJumpTableHeader zero-extends or truncates it to exactly that -- so
  // there is one shape to accept and nothing to clamp.
  getActionDefinitionsBuilder(G_JUMP_TABLE).legalFor({p0});
  getActionDefinitionsBuilder(G_BRJT).legalFor({{p0, s64}});

  // These need a custom lowering that matches what the SelectionDAG path
  // builds, so leave them to it rather than open-code a second version.
  getActionDefinitionsBuilder(
      {G_VASTART, G_VAARG, G_DYN_STACKALLOC, G_STACKSAVE, G_STACKRESTORE})
      .unsupported();

  // A fence is one instruction whatever it orders.
  getActionDefinitionsBuilder(G_FENCE).alwaysLegal();

  // A read-modify-write becomes an ldl_l/stl_c retry loop, which the
  // SelectionDAG path builds with a custom inserter -- machinery GlobalISel
  // does not run.  Marking these unsupported hands such a function to that
  // path whole, which is where they are lowered correctly; calling them legal
  // would reach a selector with nothing to select.
  getActionDefinitionsBuilder(
      {G_ATOMIC_CMPXCHG, G_ATOMIC_CMPXCHG_WITH_SUCCESS, G_ATOMICRMW_XCHG,
       G_ATOMICRMW_ADD, G_ATOMICRMW_SUB, G_ATOMICRMW_AND, G_ATOMICRMW_NAND,
       G_ATOMICRMW_OR, G_ATOMICRMW_XOR, G_ATOMICRMW_MAX, G_ATOMICRMW_MIN,
       G_ATOMICRMW_UMAX, G_ATOMICRMW_UMIN, G_ATOMICRMW_FADD, G_ATOMICRMW_FSUB,
       G_ATOMICRMW_FMAX, G_ATOMICRMW_FMIN})
      .unsupported();

  getActionDefinitionsBuilder({G_INTRINSIC, G_INTRINSIC_W_SIDE_EFFECTS,
                               G_INTRINSIC_CONVERGENT,
                               G_INTRINSIC_CONVERGENT_W_SIDE_EFFECTS})
      .alwaysLegal();
  getActionDefinitionsBuilder({G_TRAP, G_DEBUGTRAP}).alwaysLegal();

  // Note what this does not check.  verify() confirms that the rules written
  // above mention every type index they use; an opcode with no rules at all has
  // an empty rule set and passes it vacuously, so it says nothing about
  // coverage.  An uncovered opcode is found at run time instead, as "unable to
  // legalize".  What stands in for a coverage check is
  // CodeGen/Alpha/global-isel-coverage.ll, which puts each construct through
  // -global-isel-abort=1 so a missing rule is a hard error rather than a quiet
  // fall back to SelectionDAG.
  verify(*ST.getInstrInfo());
}
