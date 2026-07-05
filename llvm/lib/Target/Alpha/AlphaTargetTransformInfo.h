//===-- AlphaTargetTransformInfo.h - Alpha specific TTI ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides a TargetTransformInfoImplBase conforming object specific
// to the Alpha target machine.  It supplies Alpha-specific answers to a few
// cost queries (the slow multiplier, the software-emulated divide, the CIX
// population count and the cost of materializing a constant) and lets the
// target-independent defaults handle everything else.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHATARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_ALPHA_ALPHATARGETTRANSFORMINFO_H

#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/MathExtras.h"

namespace llvm {

class AlphaTTIImpl final : public BasicTTIImplBase<AlphaTTIImpl> {
  typedef BasicTTIImplBase<AlphaTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const AlphaSubtarget *ST;
  const AlphaTargetLowering *TLI;

  const AlphaSubtarget *getST() const { return ST; }
  const AlphaTargetLowering *getTLI() const { return TLI; }

public:
  explicit AlphaTTIImpl(const AlphaTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  // The CIX ctpop is a single instruction; without it, popcount expands to a
  // bit-twiddling sequence.
  TargetTransformInfo::PopcntSupportKind
  getPopcntSupport(unsigned TyWidth) const override {
    if (TyWidth <= 64 && ST->hasCIX())
      return TTI::PSK_FastHardware;
    return TTI::PSK_Software;
  }

  // A switch lookup table is a table of values in .rodata indexed by the
  // switch operand.  Reaching it costs a gp-relative address -- an ldah/lda
  // pair -- before the load, which is three instructions before the branch
  // even happens.  A chain of compares and branches is usually the better
  // trade here, so leave the tables unbuilt.  (Vectorization has nothing to do
  // with it: these tables are indexed one element at a time.)
  bool shouldBuildLookupTables() const override { return false; }

  // The cost of materializing an integer constant: a signed 16-bit value is one
  // lda, a 32-bit value is an ldah/lda pair, and anything wider is built from
  // shifts and several ldah/lda (or loaded from the constant pool).
  InstructionCost getIntImmCost(const APInt &Imm, Type *Ty,
                                TTI::TargetCostKind CostKind) const override {
    assert(Ty->isIntegerTy());
    unsigned BitSize = Ty->getPrimitiveSizeInBits();
    // A constant wider than a register is the most expensive kind there is:
    // each half is materialized separately, up to four ldah/lda apiece, or
    // loaded from a pool.  Reporting it free tells ConstantHoisting to leave
    // it where it is.  BitSize == 0 means the type has no size to reason
    // about, which is not a reason to call it cheap either.
    if (BitSize == 0 || BitSize > 64)
      return TTI::TCC_Expensive;

    if (Imm == 0)
      return TTI::TCC_Free;
    if (isInt<16>(Imm.getSExtValue()))
      return TTI::TCC_Basic;
    if (isInt<32>(Imm.getSExtValue()))
      return 2 * TTI::TCC_Basic;
    return 4 * TTI::TCC_Basic;
  }

  InstructionCost
  getIntImmCostInst(unsigned Opc, unsigned Idx, const APInt &Imm, Type *Ty,
                    TTI::TargetCostKind CostKind,
                    Instruction *Inst = nullptr) const override {
    return getIntImmCost(Imm, Ty, CostKind);
  }

  InstructionCost
  getIntImmCostIntrin(Intrinsic::ID IID, unsigned Idx, const APInt &Imm,
                      Type *Ty, TTI::TargetCostKind CostKind) const override {
    return getIntImmCost(Imm, Ty, CostKind);
  }

  InstructionCost getArithmeticInstrCost(
      unsigned Opcode, Type *Ty, TTI::TargetCostKind CostKind,
      TTI::OperandValueInfo Op1Info = {TTI::OK_AnyValue, TTI::OP_None},
      TTI::OperandValueInfo Op2Info = {TTI::OK_AnyValue, TTI::OP_None},
      ArrayRef<const Value *> Args = {},
      const Instruction *CxtI = nullptr) const override {
    InstructionCost Cost =
        BaseT::getArithmeticInstrCost(Opcode, Ty, CostKind, Op1Info, Op2Info);

    switch (TLI->InstructionOpcodeToISD(Opcode)) {
    default:
      return Cost;
    case ISD::MUL:
      // The multiplier is high-latency (mulq is around 7 cycles on EV6, far
      // more on earlier cores); discourage multiplies that could be strength
      // reduced or hoisted.
      return 4 * Cost;
    case ISD::SDIV:
    case ISD::UDIV:
    case ISD::SREM:
    case ISD::UREM:
      // There is no divide instruction; integer division is a call into a
      // millicode routine, so make it very expensive.
      return 64 * Cost;
    }
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHATARGETTRANSFORMINFO_H
