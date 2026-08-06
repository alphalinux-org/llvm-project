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

  // A register is a quadword and the 32-bit operations sign-extend their
  // result, so a narrower integer is kept widened to one rather than given
  // operations of its own.
  getActionDefinitionsBuilder({G_ADD, G_SUB, G_MUL, G_AND, G_OR, G_XOR})
      .legalFor({s64})
      .clampScalar(0, s64, s64)
      .widenScalarToNextPow2(0);

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
      // The s32 entry is what lets a float stay in the floating-point bank: a
      // register is a quadword, so an integer load widens to s64, but lds and
      // sts move a 32-bit float directly and convert the S_floating format on
      // the way.  Without it every float load widened to s64, landed in a
      // general register and had to reach $f0 through the stack.
      .legalForTypesWithMemDesc({{s64, p0, s8, 8},
                                 {s64, p0, s16, 8},
                                 {s64, p0, s32, 8},
                                 {s64, p0, s64, 8},
                                 {s32, p0, s32, 8},
                                 {p0, p0, s64, 8}})
      .clampScalar(0, s64, s64)
      .lower();

  getActionDefinitionsBuilder({G_SEXT, G_ZEXT, G_ANYEXT})
      .legalFor({{s64, s1}, {s64, s8}, {s64, s16}, {s64, s32}})
      .maxScalar(0, s64);

  getActionDefinitionsBuilder(G_TRUNC).alwaysLegal();

  getActionDefinitionsBuilder(G_ICMP)
      .legalFor({{s1, s64}, {s1, p0}, {s64, s64}, {s64, p0}})
      .clampScalar(0, s64, s64)
      .clampScalar(1, s64, s64);

  // A branch tests a whole register against zero, and the condition it is
  // given holds 0 or 1, so no narrowing is needed.
  getActionDefinitionsBuilder(G_BRCOND).legalFor({s1});

  getActionDefinitionsBuilder(G_PHI).legalFor({s64, p0}).clampScalar(0, s64,
                                                                     s64);
  getActionDefinitionsBuilder(G_BR).alwaysLegal();

  getActionDefinitionsBuilder({G_FADD, G_FSUB, G_FMUL, G_FDIV})
      .legalFor({s32, s64});

  // A floating compare leaves its answer in an integer register, the way an
  // integer compare does.
  getActionDefinitionsBuilder(G_FCMP)
      .legalFor({{s1, s32}, {s1, s64}, {s64, s32}, {s64, s64}})
      .clampScalar(0, s64, s64);

  getActionDefinitionsBuilder(G_FCONSTANT).legalFor({s32, s64});

  getActionDefinitionsBuilder({G_FPEXT, G_FPTRUNC})
      .legalFor({{s64, s32}, {s32, s64}});

  // A conversion goes through a whole register: the value is moved between the
  // banks and converted there, so the integer side is always 64 bits.
  getActionDefinitionsBuilder(G_SITOFP)
      .legalFor({{s32, s64}, {s64, s64}})
      .clampScalar(1, s64, s64);

  getActionDefinitionsBuilder(G_FPTOSI)
      .legalFor({{s64, s32}, {s64, s64}})
      .clampScalar(0, s64, s64);

  // A conditional move leaves its destination alone when the condition is zero,
  // so the false value is what the destination already holds.
  // A 32-bit choice is legal as it stands: cmovne and fcmovne both move whole
  // registers and care nothing for the width of what is in them.  Widening it
  // would put a float through a pair of integer registers and cost a round trip
  // through memory at each end.
  getActionDefinitionsBuilder(G_SELECT)
      .legalFor({{s32, s1}, {s64, s1}, {p0, s1}})
      .clampScalar(0, s32, s64);

  // There is no unsigned conversion instruction; both directions are built out
  // of the signed one.
  getActionDefinitionsBuilder(G_UITOFP).clampScalar(1, s64, s64).lower();

  getActionDefinitionsBuilder(G_FPTOUI).clampScalar(0, s64, s64).lower();

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
  getActionDefinitionsBuilder({G_FNEG, G_FABS}).legalFor({s32, s64});
  // cpys takes the sign from one register and the rest from another, so both
  // operands are named.
  getActionDefinitionsBuilder(G_FCOPYSIGN)
      .legalFor({{s32, s32}, {s64, s64}})
      .lower();

  // sqrts and sqrtt are FIX instructions; everything else is a libcall.
  getActionDefinitionsBuilder(G_FSQRT)
      .legalFor(ST.hasFIX(), {s32, s64})
      .libcall();
  getActionDefinitionsBuilder(
      {G_FMA, G_FREM, G_FPOW, G_FEXP, G_FEXP2, G_FLOG, G_FLOG2, G_FLOG10,
       G_FSIN, G_FCOS, G_FTAN, G_FCEIL, G_FFLOOR, G_FRINT, G_FNEARBYINT,
       G_INTRINSIC_TRUNC, G_INTRINSIC_ROUND, G_INTRINSIC_ROUNDEVEN})
      .libcall();
  getActionDefinitionsBuilder({G_FMINNUM, G_FMAXNUM}).libcall();

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
  // gp-relative way a global's is, which the selector does not do for these
  // two, so they go to the SelectionDAG path as well.  The only thing that
  // reaches G_BRINDIRECT is an indirectbr, whose target is a blockaddress, so
  // it goes the same way; calling it legal would be a claim the selector has no
  // case, no GINodeEquiv and no brind pattern to back up.
  getActionDefinitionsBuilder({G_BRINDIRECT, G_BLOCK_ADDR, G_CONSTANT_POOL})
      .unsupported();

  // A jump table dispatch is the gp-relative sequence LowerBR_JT builds, which
  // the selector has no counterpart for.  Say so rather than call it legal:
  // unsupported hands the function to the SelectionDAG path, where it is
  // lowered correctly, while claiming legality would reach the selector and
  // fail there.
  getActionDefinitionsBuilder({G_BRJT, G_JUMP_TABLE}).unsupported();

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
