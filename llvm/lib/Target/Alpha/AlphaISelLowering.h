//===-- AlphaISelLowering.h - Alpha DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that Alpha uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H
#define LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class AlphaSubtarget;
class AlphaTargetMachine;

namespace AlphaISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  // Return with a glue-connected chain of copies into the return registers.
  RET_GLUE,

  // Wraps a global address whose value is loaded from its GOT slot with an
  // R_ALPHA_LITERAL relocation.
  LITERAL,

  // A function call through the procedure value in $27.
  CALL,

  // GP-relative address parts, materialized with ldah !gprelhigh and
  // lda !gprellow.
  GPREL_HI,
  GPREL_LO,
};
} // namespace AlphaISD

class AlphaTargetLowering : public TargetLowering {
public:
  AlphaTargetLowering(const AlphaTargetMachine &TM, const AlphaSubtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  // Comparisons produce a 0/1 result in a 64-bit integer register.
  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override {
    return MVT::i64;
  }

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  // Anything that does not fit the single return register comes back in memory
  // through a hidden pointer, as it does under GCC (alpha_return_in_memory
  // returns true for every value wider than a word).  Returning false here is
  // what makes the caller allocate the buffer and pass it in $16.
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

private:
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;

  const AlphaSubtarget &Subtarget;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H
