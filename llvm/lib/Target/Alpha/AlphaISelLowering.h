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
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
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

  // A tail call: jump to the callee (procedure value in $27) without saving a
  // return address, so it returns straight to our caller.
  TC_RETURN,

  // The -msmall-text form of a tail call: a single PC-relative br to the
  // callee, which like jmp discards the return address.
  TC_RETURN_BR,

  // A direct call: like CALL, but carries the callee symbol so the jsr can be
  // tagged with a branch-prediction hint and a lituse_jsr relocation.
  CALL_DIRECT,

  // A direct call to a dso-local callee: the jsr carries only the lituse_jsr
  // relocation (no hint), which lets the linker relax the call to a bsr.
  CALL_DIRECT_LOCAL,

  // A direct call emitted as a single PC-relative bsr under -msmall-text, where
  // the callee is in range and shares the global pointer, so no procedure value
  // is loaded and the global pointer is not reloaded afterwards.
  CALL_BSR,

  // Calls to __tls_get_addr in the general- and local-dynamic TLS sequences.
  // The jsr carries a lituse_tlsgd or lituse_tlsldm relocation, letting the
  // linker relax the sequence to initial- or local-exec when possible.
  CALL_TLSGD,
  CALL_TLSLDM,

  // A call to a division millicode routine (entered through $23).
  DIVCALL,

  // GP-relative address parts, materialized with ldah !gprelhigh and
  // lda !gprellow.
  GPREL_HI,
  GPREL_LO,

  // Single-precision integer/floating register move (itofs/ftois).
  MOVI2F_S,
  MOVF2I_S,

  // Conditional branches that test a register against zero: equal, not-equal,
  // the signed relations, and the low-bit tests.
  BR_EQ,
  BR_NE,
  BR_LT,
  BR_LE,
  BR_GT,
  BR_GE,

  // Thread-pointer-relative address parts for local-exec TLS, materialized
  // with ldah !tprelhi and lda !tprello.
  TPREL_HI,
  TPREL_LO,

  // Initial-exec TLS: the thread-pointer-relative offset loaded from the GOT
  // with ldq !gottprel.
  GOTTPREL,

  // General-dynamic TLS: the address of the tlsgd descriptor (lda !tlsgd)
  // passed to __tls_get_addr.
  TLSGD,

  // Local-dynamic TLS: the module descriptor (lda !tlsldm) passed to
  // __tls_get_addr, and the module-relative address parts (ldah !dtprelhi /
  // lda !dtprello).
  TLSLDM,
  DTPREL_HI,
  DTPREL_LO,

  // Everything from here down carries a memory operand and is built with
  // getMemIntrinsicNode, which only accepts an opcode the target claims as a
  // memory one (see AlphaSelectionDAGInfo).  Keep them contiguous and keep the
  // two bounds at the end of the list pointing at the first and last of them.

  // Unaligned access primitives.  LDQ_U/STQ_U load/store the aligned quadword
  // containing an address, ignoring its low three bits, used to build the
  // ldq_u + extract / insert + stq_u sequences for misaligned loads and stores.
  LDQ_U,
  STQ_U,

  // A misaligned store, kept whole until it is expanded into a bundle.
  USTORE,

  // A -msafe-partial misaligned store (value, pointer, byte width): each
  // spanned quadword is updated with a lock-based ldq_l/stq_c loop so the
  // read-modify-write is safe against concurrent access to adjacent bytes.
  SAFE_USTORE,

  LAST_MEMORY_OPCODE = SAFE_USTORE,
};
} // namespace AlphaISD

// Without this a target is taken to have no memory opcodes at all, and
// getMemIntrinsicNode refuses to build one of ours.
class AlphaSelectionDAGInfo : public SelectionDAGTargetInfo {
public:
  bool isTargetMemoryOpcode(unsigned Opcode) const override {
    return Opcode >= AlphaISD::LDQ_U &&
           Opcode <= AlphaISD::LAST_MEMORY_OPCODE;
  }
};

class AlphaTargetLowering : public TargetLowering {
public:
  AlphaTargetLowering(const AlphaTargetMachine &TM, const AlphaSubtarget &STI);

  const char *getTargetNodeName(unsigned Opcode) const override;

  // Inline assembly: "r" selects an integer register, "f" a floating-point one.
  ConstraintType getConstraintType(StringRef Constraint) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  Register getRegisterByName(const char *RegName, LLT Ty,
                             const MachineFunction &MF) const override;
  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

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

  // The comparand has to be extended the same way, or the success flag compares
  // a sign-extended loaded value against an any-extended operand and a negative
  // narrow comparand never reports success.
  ISD::NodeType getExtendForAtomicCmpSwapArg() const override {
    return ISD::SIGN_EXTEND;
  }

  // +0.0, -0.0 and +2.0 are producible in a single instruction (cpys/cpysn of
  // the zero register, or cmpteq $f31,$f31), so keep them as immediates rather
  // than constant-pool loads.
  // $f31 reads as +0.0, its negation gives -0.0 and cmpteq of it with itself
  // gives +2.0, so those three need no constant pool.  X_floating has none of
  // that: it lives in memory and every operation on it is a call, so a constant
  // of that type is never immediate.
  bool isFPImmLegal(const APFloat &Imm, EVT VT,
                    bool ForCodeSize) const override {
    if (VT != MVT::f32 && VT != MVT::f64)
      return false;
    return Imm.isExactlyValue(+0.0) || Imm.isExactlyValue(-0.0) ||
           Imm.isExactlyValue(+2.0);
  }

  // lds, which is how a float is loaded, maps the S_floating exponent onto the
  // T_floating one, and that mapping has no entry for a denormal: an exponent
  // of zero stays zero and the value read back is not the one written. A double
  // may therefore only be shrunk into a float that is normal (or a zero, which
  // does survive the mapping).
  bool ShouldShrinkFPConstant(EVT VT, const APFloat &Val) const override {
    return !Val.isDenormal();
  }

  // The multiplier is slow, so split a multiply by a constant of the form
  // 2^N +/- 1 (or a shifted such value) into a shift and an add/subtract.
  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  // Truncating an integer to a narrower one keeps the low bits already in the
  // register, so it costs nothing.
  bool isTruncateFree(Type *Ty1, Type *Ty2) const override {
    if (!Ty1->isIntegerTy() || !Ty2->isIntegerTy())
      return false;
    unsigned From = Ty1->getPrimitiveSizeInBits();
    unsigned To = Ty2->getPrimitiveSizeInBits();
    return From > To && From <= 64;
  }
  bool isTruncateFree(EVT VT1, EVT VT2) const override {
    if (!VT1.isInteger() || !VT2.isInteger())
      return false;
    unsigned From = VT1.getSizeInBits();
    unsigned To = VT2.getSizeInBits();
    return From > To && From <= 64;
  }

  // A value narrower than a register is held sign-extended (addl/ldl and the
  // like sign-extend), so a sign extension is a single instruction while a zero
  // extension needs an extra zapnot; prefer the sign extension.
  // Only i32 -> i64.  A 32-bit value is held sign-extended, so widening one is
  // free -- ldl and addl already produce that form -- while zero-extending it
  // costs a zapnot.  Every narrower width goes the other way: an i1 is already
  // 0 or 1 so zero-extending it is free where sign-extending costs a subq, and
  // an i8 or i16 zero-extends with a single zapnot but sign-extends with sextb
  // or sextw only when BWX is present, and with an sll/sra pair otherwise.
  bool isSExtCheaperThanZExt(EVT FromTy, EVT ToTy) const override {
    return FromTy == MVT::i32 && ToTy == MVT::i64;
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
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  SDValue LowerCall(CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  // Let CodeGenPrepare duplicate a return into a tail-marked call's block so
  // the call lands in tail position; LowerCall applies the real eligibility
  // checks.
  bool mayBeEmittedAsTailCall(const CallInst *CI) const override {
    return CI->isTailCall();
  }

private:
  // A call in tail position can reuse the caller's frame and return slot when
  // it passes nothing on the stack, passes nothing that lives in the frame we
  // are about to tear down, and shares the caller's C calling convention.
  bool isEligibleForTailCallOptimization(
      CallingConv::ID CallerCC, CallingConv::ID CalleeCC, bool IsVarArg,
      unsigned NumStackBytes,
      const SmallVectorImpl<ISD::OutputArg> &Outs) const;

  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBITCAST(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerLOAD(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSTORE(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerBR_JT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerDivRem(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerMULHS(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVAARG(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerVACOPY(SDValue Op, SelectionDAG &DAG) const;

  const AlphaSubtarget &Subtarget;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_ALPHAISELLOWERING_H
