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

#include "llvm/CodeGen/MachineJumpTableInfo.h"
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

  // A call to a division millicode routine (entered through $23).
  DIVCALL,

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

  // Jump-table entries are absolute 64-bit block addresses.
  unsigned getJumpTableEncoding() const override {
    return MachineJumpTableInfo::EK_BlockAddress;
  }

  // Comparisons produce a 0/1 result in a 64-bit integer register.
  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override {
    return MVT::i64;
  }

  // Atomic loads/stores are plain aligned accesses; the atomic expander adds
  // memory barriers around the stronger orderings.
  //
  // This must keep returning true.  An acquire load lowering to `ldq; mb` is
  // what makes a plain load that follows one ordered against it, and code that
  // reads a pointer published by another thread and then dereferences it -- the
  // openmp runtime's TCR_SYNC_PTR, among others -- relies on that.  Alpha is
  // the one architecture where such a dependent load is not ordered by the
  // dependency alone, so a bare `ldq` here would break that code silently.
  bool shouldInsertFencesForAtomic(const Instruction *I) const override {
    return true;
  }

  // The default emits a leading fence only for an instruction that stores, so a
  // sequentially consistent load came out as a bare `ldq; mb'.  That leaves no
  // barrier between an earlier SC store and this load, which is exactly the
  // store-buffer shape: two threads each storing to one location and then
  // loading the other may both read the stale value, which sequential
  // consistency forbids.  PowerPC overrides this for the same reason.
  Instruction *emitLeadingFence(IRBuilderBase &Builder, Instruction *Inst,
                                AtomicOrdering Ord) const override;
  Instruction *emitTrailingFence(IRBuilderBase &Builder, Instruction *Inst,
                                 AtomicOrdering Ord) const override;

  // The read-modify-writes that have an ldq_l/stq_c inserter stay as target
  // nodes; everything else -- the min/max forms, nand, the floating-point and
  // wrapping ones -- has no pattern, so let the atomic expander open-code it as
  // a compare-and-swap loop around the cmpxchg lowering below.
  AtomicExpansionKind
  shouldExpandAtomicRMWInIR(const AtomicRMWInst *AI) const override {
    switch (AI->getOperation()) {
    case AtomicRMWInst::Xchg:
    case AtomicRMWInst::Add:
    case AtomicRMWInst::Sub:
    case AtomicRMWInst::And:
    case AtomicRMWInst::Or:
    case AtomicRMWInst::Xor:
      return AtomicExpansionKind::None;
    default:
      return AtomicExpansionKind::CmpXChg;
    }
  }

  AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(const AtomicCmpXchgInst *AI) const override {
    return AtomicExpansionKind::None;
  }

  // Alpha holds a value narrower than a register in sign-extended form, and the
  // loop below produces its result that way, so say so: otherwise a narrow
  // atomic result is compared against a constant materialized zero-extended and
  // a negative one never matches.
  ISD::NodeType getExtendForAtomicOps() const override {
    return ISD::SIGN_EXTEND;
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

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

private:
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_JT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerDivRem(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVACOPY(SDValue Op, SelectionDAG &DAG) const;

  const AlphaSubtarget &Subtarget;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H
