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

  // Whether the return value fits the registers the convention gives it: one
  // integer register and two floating ones.  What does not is returned in
  // memory through a hidden pointer, and returning false here is what makes
  // the caller allocate the buffer and pass it in $16.
  //
  // This is not "wider than a register": GCC's alpha_return_in_memory judges a
  // complex float by the size of one part rather than of the pair, so a 16-byte
  // _Complex double comes back in $f0/$f1, and RetCC_Alpha does the same.  What
  // it does send to memory is every aggregate, every float vector, and anything
  // else whose one part is wider than a word.
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *MBB) const override;

private:
  const AlphaSubtarget &Subtarget;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H
