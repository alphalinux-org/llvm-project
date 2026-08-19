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
      .legalForTypesWithMemDesc({{s64, p0, s8, 8},
                                 {s64, p0, s16, 8},
                                 {s64, p0, s32, 8},
                                 {s64, p0, s64, 8},
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

  getActionDefinitionsBuilder(G_FCONSTANT).legalFor({s32, s64});

  getActionDefinitionsBuilder({G_FPEXT, G_FPTRUNC})
      .legalFor({{s64, s32}, {s32, s64}});

  // The memory intrinsics become libcalls, as they do on the SelectionDAG
  // path.
  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE, G_MEMSET}).libcall();
  getActionDefinitionsBuilder({G_MEMCPY_INLINE, G_MEMSET_INLINE}).lower();

  verify(*ST.getInstrInfo());
}
