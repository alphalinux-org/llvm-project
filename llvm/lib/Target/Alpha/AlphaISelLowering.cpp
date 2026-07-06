//===-- AlphaISelLowering.cpp - Alpha DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaISelLowering.h"
#include "Alpha.h"
#include "AlphaInstrInfo.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/GlobalIFunc.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsAlpha.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "AlphaGenCallingConv.inc"

AlphaTargetLowering::AlphaTargetLowering(const AlphaTargetMachine &TM,
                                         const AlphaSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // Set up the register classes.  With -mno-fp-regs the floating-point file is
  // not available at all, so no floating type has a register class and every
  // floating operation is softened into a libcall -- which is what GCC's
  // -mno-fp-regs does, and what the kernel this option exists for expects.
  addRegisterClass(MVT::i64, &Alpha::GPRCRegClass);
  if (!STI.hasNoFPRegs()) {
    addRegisterClass(MVT::f32, &Alpha::FPRCRegClass);
    addRegisterClass(MVT::f64, &Alpha::FPRCRegClass);
  }

  setStackPointerRegisterToSaveRestore(Alpha::R30);

  // Comparison instructions leave 0 or 1 in the destination register.
  setBooleanContents(ZeroOrOneBooleanContent);

  // select maps to a conditional move; expand SELECT_CC into SETCC + SELECT.
  for (MVT VT : {MVT::i64, MVT::f32, MVT::f64}) {
    setOperationAction(ISD::SELECT, VT, Legal);
    setOperationAction(ISD::SELECT_CC, VT, Expand);
  }

  // Integer BR_CC is custom-lowered so that a comparison against zero becomes a
  // single test-and-branch (beq/bne/blt/ble/bgt/bge).  Floating-point BR_CC is
  // expanded into an fcmp (a 0/1 result) followed by bne.
  setOperationAction(ISD::BR_CC, MVT::i64, Custom);
  setOperationAction(ISD::BR_CC, MVT::f32, Expand);
  setOperationAction(ISD::BR_CC, MVT::f64, Expand);
  // Jump tables are emitted as GP-relative offset tables and dispatched with a
  // load and an indirect jump.
  setOperationAction(ISD::JumpTable, MVT::i64, Custom);
  setOperationAction(ISD::BlockAddress, MVT::i64, Custom);
  setOperationAction(ISD::RETURNADDR, MVT::i64, Custom);
  setOperationAction(ISD::FRAMEADDR, MVT::i64, Custom);
  setOperationAction(ISD::BR_JT, MVT::Other, Custom);

  // Aligned integer loads and stores are atomic; barriers are inserted around
  // stronger orderings.  Wider atomic read-modify-writes are not handled yet.
  setMaxAtomicSizeInBitsSupported(64);

  // A fence needs looking at before it becomes an mb: see LowerOperation.
  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);
  // Variadic function support.
  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Custom);
  setOperationAction(ISD::VACOPY, MVT::Other, Custom);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  // Global addresses are loaded from the GOT; constant pools are GP-relative.
  setOperationAction(ISD::GlobalAddress, MVT::i64, Custom);
  setOperationAction(ISD::GlobalTLSAddress, MVT::i64, Custom);
  setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i64, Custom);
  // Save/restore of the stack pointer expand to a copy from/to $30.
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);
  setOperationAction(ISD::ConstantPool, MVT::i64, Custom);

  // Alpha has no misaligned load/store; a misaligned word/long/quad access is
  // custom-lowered to an ldq_u + extract / insert + stq_u sequence.  Word and
  // longword accesses reach this path as extending loads and truncating stores,
  // since i16 and i32 are not legal register types.
  setOperationAction(ISD::LOAD, MVT::i64, Custom);
  setOperationAction(ISD::STORE, MVT::i64, Custom);

  // Software-directed prefetch lowers to a load whose destination is R31/F31,
  // but only where that is a real prefetch; drop it otherwise.
  setOperationAction(ISD::PREFETCH, MVT::Other, Custom);

  // Bit-cast between f32 and i32 uses the single-precision integer/FP move
  // (itofs/ftois) rather than a stack bounce.  i32 is illegal, so the i32
  // result form is handled in ReplaceNodeResults and the f32 result form here.
  // With no floating registers the value is already in an integer one and the
  // bit-cast is nothing at all, so the moves must not be asked for.
  if (!STI.hasNoFPRegs()) {
    setOperationAction(ISD::BITCAST, MVT::f32, Custom);
    setOperationAction(ISD::BITCAST, MVT::i32, Custom);
  }
  for (MVT VT : {MVT::i16, MVT::i32}) {
    setLoadExtAction(ISD::EXTLOAD, MVT::i64, VT, Custom);
    setLoadExtAction(ISD::ZEXTLOAD, MVT::i64, VT, Custom);
    setTruncStoreAction(MVT::i64, VT, Custom);
  }
  // Only the longword has a sign-extending load of its own; a signed word load
  // is expanded further down into an extending load and a sign-extension, so
  // asking for it here would be overwritten there.
  setLoadExtAction(ISD::SEXTLOAD, MVT::i64, MVT::i32, Custom);

  // Unsigned integer/floating conversions expand through the signed ones.
  setOperationAction(ISD::UINT_TO_FP, MVT::i64, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::i64, Expand);

  // There is no integer division instruction; call the division millicode.
  for (auto Op : {ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM})
    setOperationAction(Op, MVT::i64, Custom);

  // There is no half-precision support; convert to/from f16 with libcalls and
  // split f16 loads/stores into an i16 access plus a conversion.
  setOperationAction(ISD::FP16_TO_FP, MVT::f32, Expand);
  setOperationAction(ISD::FP_TO_FP16, MVT::f32, Expand);
  setOperationAction(ISD::FP16_TO_FP, MVT::f64, Expand);
  setOperationAction(ISD::FP_TO_FP16, MVT::f64, Expand);
  for (auto VT : {MVT::f32, MVT::f64}) {
    setLoadExtAction(ISD::EXTLOAD, VT, MVT::f16, Expand);
    setTruncStoreAction(VT, MVT::f16, Expand);
  }

  // With BWX, sextb/sextw sign-extend a byte/word in one instruction; otherwise
  // sign-extend-in-register expands to an sll/sra pair.
  for (auto VT : {MVT::i8, MVT::i16})
    setOperationAction(ISD::SIGN_EXTEND_INREG, VT,
                       STI.hasBWX() ? Legal : Expand);
  // No single-bit sign-extend instruction either.  This one does not expand
  // to the shift pair above: a single bit is masked off with and and negated
  // with subq.
  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  // No byte-swap or rotate instructions; expand to shift/mask sequences.
  setOperationAction(ISD::BSWAP, MVT::i64, Expand);
  setOperationAction(ISD::ROTL, MVT::i64, Expand);
  setOperationAction(ISD::ROTR, MVT::i64, Expand);

  // umulh provides the high half of an unsigned 64x64 multiply; the signed high
  // multiply is expanded in terms of it.
  setOperationAction(ISD::MULHU, MVT::i64, Legal);
  // No signed high-multiply instruction, but synthesizing one from umulh is
  // still far cheaper than the millicode divide, so keep MULHS available (as a
  // custom expansion) to enable magic-number signed division by a constant.
  setOperationAction(ISD::MULHS, MVT::i64, Custom);
  setOperationAction(ISD::SMUL_LOHI, MVT::i64, Expand);
  setOperationAction(ISD::UMUL_LOHI, MVT::i64, Expand);

  // The CIX extension (ev67) provides population/leading/trailing zero counts;
  // otherwise they are expanded to bit-twiddling sequences.
  for (auto Op : {ISD::CTPOP, ISD::CTLZ, ISD::CTTZ})
    setOperationAction(Op, MVT::i64, STI.hasCIX() ? Legal : Expand);

  // The FIX extension (ev6) provides square-root instructions; without it,
  // fsqrt becomes a libcall.
  for (auto VT : {MVT::f32, MVT::f64})
    setOperationAction(ISD::FSQRT, VT, STI.hasFIX() ? Legal : Expand);

  // Alpha has no instructions for these; expand them (to libcalls such as sin,
  // pow, floor, fmod and fma, or to compare/select sequences for min/max).
  for (auto VT : {MVT::f32, MVT::f64})
    for (auto Op :
         {ISD::FSIN,       ISD::FCOS,    ISD::FSINCOS,    ISD::FTAN,
          ISD::FPOW,       ISD::FPOWI,   ISD::FEXP,       ISD::FEXP2,
          ISD::FEXP10,     ISD::FLOG,    ISD::FLOG2,      ISD::FLOG10,
          ISD::FFLOOR,     ISD::FCEIL,   ISD::FTRUNC,     ISD::FRINT,
          ISD::FNEARBYINT, ISD::FROUND,  ISD::FROUNDEVEN, ISD::FMA,
          ISD::FREM,       ISD::FMINNUM, ISD::FMAXNUM,    ISD::FMINIMUM,
          ISD::FMAXIMUM})
      setOperationAction(Op, VT, Expand);

  // Only the ordered floating-point comparisons have direct instructions; the
  // rest are expanded into combinations of them.  SETNE belongs here too: the
  // NaN-agnostic codes otherwise map straight onto cmpteq/cmptlt/cmptle, but
  // there is no cmptne, so it has to become an inverted cmpteq.
  for (MVT VT : {MVT::f32, MVT::f64})
    for (auto CC :
         {ISD::SETONE, ISD::SETUEQ, ISD::SETUGT, ISD::SETUGE, ISD::SETULT,
          ISD::SETULE, ISD::SETUNE, ISD::SETUO, ISD::SETO, ISD::SETNE})
      setCondCodeAction(CC, VT, Expand);

  // Signed byte/word loads become a zero/any-extending load plus an explicit
  // sign-extension, so only the extending byte/word loads need instructions.
  for (MVT VT : {MVT::i8, MVT::i16})
    setLoadExtAction(ISD::SEXTLOAD, MVT::i64, VT, Expand);

  // A boolean occupies a byte in memory; extend an i1 load through an i8 load.
  for (auto Ext : {ISD::EXTLOAD, ISD::ZEXTLOAD, ISD::SEXTLOAD})
    setLoadExtAction(Ext, MVT::i64, MVT::i1, Promote);

  setTargetDAGCombine(ISD::MUL);

  computeRegisterProperties(STI.getRegisterInfo());
}

// Estimate how many instructions build x * V from x with shifts and scaled
// adds; 100 (effectively infinite) means it would still need a multiply.  The
// recursion mirrors the factoring in PerformDAGCombine and the generic
// shift-and-add decomposition it defers to.
static unsigned mulSeqCost(uint64_t V) {
  unsigned Tz = llvm::countr_zero(V);
  uint64_t Odd = V >> Tz;
  if (Odd == 1)
    return Tz ? 1 : 0; // a power of two is a single shift (or the value itself)
  if (isPowerOf2_64(Odd - 1) || isPowerOf2_64(Odd + 1))
    return Tz ? 2 : 1; // 2^k +/- 1 is a scaled add, plus a shift if scaled up
  for (uint64_t F : {9, 5, 3})
    if (Odd % F == 0)
      return 1 + mulSeqCost(V / F); // one scaled add, then the cofactor
  return 100;
}

// Factor a multiply by a constant whose odd part is not 2^k +/- 1 (so the
// generic shift-and-add decomposition does not apply) into two smaller
// multiplies, when a factor of 3, 5 or 9 can be peeled off.  Each of those is a
// single scaled add/subtract (s4subq/s4addq/s8addq), and the remaining multiply
// is decomposed in turn, so the whole product costs a few dependent ALU ops
// instead of the high-latency multiplier.
SDValue AlphaTargetLowering::PerformDAGCombine(SDNode *N,
                                               DAGCombinerInfo &DCI) const {
  if (N->getOpcode() != ISD::MUL || N->getValueType(0) != MVT::i64)
    return SDValue();
  auto *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C || DCI.isBeforeLegalizeOps())
    return SDValue();

  // Factor the magnitude and negate afterwards.  A negative constant taken
  // zero-extended is astronomically large, its estimated chain length runs
  // past the cut-off below, and the multiply would be left for the
  // multiplier -- while decomposeMulByConstant, which does use the magnitude,
  // already handles the 2^k +/- 1 negatives.  The negation of the result is a
  // single subq from $31.
  bool Negate = C->getAPIntValue().isNegative();
  uint64_t V = Negate ? (-C->getAPIntValue()).getZExtValue()
                      : C->getZExtValue();
  if (V < 2)
    return SDValue();
  // The odd part: the trailing power of two is a free shift the combiner
  // already forms.
  uint64_t Odd = V >> llvm::countr_zero(V);
  // Already a single shift-and-add (2^k +/- 1), or a plain power of two.
  if (Odd == 1 || isPowerOf2_64(Odd - 1) || isPowerOf2_64(Odd + 1))
    return SDValue();
  // Only worth it when the shift/add chain is clearly shorter than the
  // multiplier's latency; otherwise leave the multiply in place.
  if (mulSeqCost(V) > 4)
    return SDValue();
  // Peel off the largest single-instruction scaled factor.
  uint64_t Factor = 0;
  for (uint64_t F : {9, 5, 3})
    if (Odd % F == 0) {
      Factor = F;
      break;
    }
  if (!Factor)
    return SDValue();

  // Emit the factor as an explicit scaled add/subtract rather than a nested
  // multiply: 3x = 4x - x, 5x = 4x + x, 9x = 8x + x.  A nested multiply would
  // be reassociated back into the original constant and re-enter this combine;
  // the shift/add form breaks that cycle and selects to s4subq/s4addq/s8addq.
  // The remaining multiply by V / Factor is decomposed (or factored) in turn.
  SDLoc DL(N);
  SelectionDAG &DAG = DCI.DAG;
  SDValue X = N->getOperand(0);
  SDValue Shl = DAG.getNode(ISD::SHL, DL, MVT::i64, X,
                            DAG.getConstant(Factor == 9 ? 3 : 2, DL, MVT::i64));
  SDValue Scaled =
      DAG.getNode(Factor == 3 ? ISD::SUB : ISD::ADD, DL, MVT::i64, Shl, X);
  SDValue Mul = DAG.getNode(ISD::MUL, DL, MVT::i64, Scaled,
                            DAG.getConstant(V / Factor, DL, MVT::i64));
  if (!Negate)
    return Mul;
  return DAG.getNode(ISD::SUB, DL, MVT::i64, DAG.getConstant(0, DL, MVT::i64),
                     Mul);
}

bool AlphaTargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                                 SDValue C) const {
  // Only the 64-bit multiply is a candidate; narrower multiplies are promoted
  // to it.
  if (VT != MVT::i64)
    return false;

  const APInt &Imm = cast<ConstantSDNode>(C)->getAPIntValue();

  // x * 3 is a single s4subq (4x - x); an isel pattern handles it, so keep it
  // as a multiply here rather than splitting it into a shift and subtract.
  if (Imm == 3)
    return false;

  // Split when the constant, after removing trailing zeros, is one away from a
  // power of two: the DAGCombiner then emits (shl x, N) +/- x, which folds into
  // an s4addq/s8addq/s8subq when N is 2 or 3.
  APInt MulC = Imm.abs();
  MulC.lshrInPlace(MulC.countr_zero());
  return (MulC - 1).isPowerOf2() || (MulC + 1).isPowerOf2();
}

TargetLowering::ConstraintType
AlphaTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    // Constraint letters match GCC's alpha back end.
    switch (Constraint[0]) {
    case 'f':
      // Under -mno-fp-regs there is no floating-point register class to bind
      // to.  Fall through to the generic handling, which rejects the operand
      // with a diagnostic rather than reaching an assertion about copying to
      // an illegal type.  gcc reports "impossible constraint in 'asm'" here.
      if (Subtarget.hasNoFPRegs())
        break;
      return C_RegisterClass;
    case 'I': // Unsigned 8-bit constant.
    case 'J': // The constant zero.
    case 'K': // Signed 16-bit constant.
    case 'L': // ldah constant.
    case 'M': // zap byte-mask constant.
    case 'N': // Complemented unsigned 8-bit constant.
    case 'O': // Negated unsigned 8-bit constant.
    case 'P': // The constant 1, 2 or 3.
    case 'S': // Unsigned 6-bit constant.
      return C_Immediate;
    case 'Q': // Memory operand.
    case 'R': // Symbolic operand.
    case 'U': // Memory address operand.
      return C_Memory;
    }
  }
  return TargetLowering::getConstraintType(Constraint);
}

void AlphaTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  if (Constraint.size() == 1 &&
      StringRef("IJKLMNOPS").contains(Constraint[0])) {
    // The constant-integer constraints (GCC's alpha letters).  Enforce each
    // letter's range so an out-of-range value is rejected here and the register
    // alternative of a combined constraint such as "rI"/"rJ" is used instead.
    auto *C = dyn_cast<ConstantSDNode>(Op);
    if (!C)
      return;
    int64_t V = C->getSExtValue();
    uint64_t U = C->getZExtValue();
    bool Ok = false;
    switch (Constraint[0]) {
    case 'I':
      Ok = isUInt<8>(U);
      break; // unsigned 8-bit
    case 'J':
      Ok = V == 0;
      break; // zero
    case 'K':
      Ok = isInt<16>(V);
      break; // lda
    case 'L':
      Ok = (U & 0xffffULL) == 0 && isInt<32>(V);
      break; // ldah
    case 'N':
      Ok = isUInt<8>(~U);
      break; // complemented 8-bit
    case 'O':
      Ok = isUInt<8>(0 - U);
      break; // negated 8-bit
    case 'P':
      Ok = V == 1 || V == 2 || V == 3;
      break; // 1, 2 or 3
    case 'S':
      Ok = isUInt<6>(U);
      break; // 6-bit shift count
    case 'M':
      // A zap byte-mask: each byte of the value is either kept whole or
      // cleared, which is what `zapnot' can make of a register.
      Ok = true;
      for (unsigned I = 0; I != 8; ++I) {
        uint8_t B = (U >> (I * 8)) & 0xff;
        if (B != 0 && B != 0xff)
          Ok = false;
      }
      break;
    }
    if (Ok)
      Ops.push_back(
          DAG.getSignedTargetConstant(V, SDLoc(Op), Op.getValueType()));
    return;
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

std::pair<unsigned, const TargetRegisterClass *>
AlphaTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
      return std::make_pair(0U, &Alpha::GPRCRegClass);
    case 'f':
      // Nothing to bind to under -mno-fp-regs; see getConstraintType.
      if (Subtarget.hasNoFPRegs())
        break;
      // Use a single-value-type register class so the operand's value type is
      // unambiguous (the shared FPRC would default to f32 and mishandle f64).
      // An integer bound to an FP register (e.g. loading the FPCR) is 64-bit,
      // so treat i64 like f64.
      if (VT == MVT::f64 || VT == MVT::i64)
        return std::make_pair(0U, &Alpha::F8RCRegClass);
      return std::make_pair(0U, &Alpha::F4RCRegClass);
    }
  }

  // An explicit physical register named numerically, e.g. {$0} or {$f0}.  The
  // kernel binds register variables to specific registers this way for its
  // PAL-call and syscall sequences.
  if (Constraint.size() > 2 && Constraint.front() == '{' &&
      Constraint.back() == '}') {
    StringRef Name = Constraint.substr(1, Constraint.size() - 2);
    unsigned N;
    if (Name.consume_front("$")) {
      bool IsFP = Name.consume_front("f");
      if (!Name.getAsInteger(10, N) && N < 32) {
        // Under -mno-fp-regs there is nothing to bind {$fN} to either -- the
        // floating-point file is out of the target's register classes, so the
        // operand's type is not legal and handing back a class for it asserts
        // in getCopyToParts.  Decline it here and let the generic code report
        // the constraint as unsatisfiable, which is what the `f' constraint
        // above already does.
        if (IsFP && Subtarget.hasNoFPRegs())
          return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint,
                                                              VT);
        if (IsFP)
          return std::make_pair((unsigned)(Alpha::F0 + N),
                                VT == MVT::f64 ? &Alpha::F8RCRegClass
                                               : &Alpha::F4RCRegClass);
        return std::make_pair((unsigned)(Alpha::R0 + N), &Alpha::GPRCRegClass);
      }
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

Register
AlphaTargetLowering::getRegisterByName(const char *RegName, LLT /*Ty*/,
                                       const MachineFunction &MF) const {
  // Accept the numeric register spelling used for global register variables,
  // e.g. the kernel's  register T *p __asm__("$8");  and floating "$f0".
  StringRef Name(RegName);
  unsigned N;
  Register Reg;
  if (Name.consume_front("$")) {
    bool IsFP = Name.consume_front("f");
    if (!Name.getAsInteger(10, N) && N < 32)
      Reg = IsFP ? Alpha::F0 + N : Alpha::R0 + N;
  }
  if (!Reg)
    reportFatalUsageError(Twine("Invalid register name \"") + RegName + "\".");

  // The register has to be out of the allocator's reach, or the value the
  // variable names is whatever the allocator last put there.  A register is out
  // of reach because the ABI reserves it ($29, $30, $31, the frame pointer) or
  // because the user reserved it with -ffixed-$<n>; this is the same check
  // RISCV and AArch64 make.
  const AlphaRegisterInfo *TRI = Subtarget.getRegisterInfo();
  if (!TRI->getReservedRegs(MF).test(Reg.id()))
    reportFatalUsageError(Twine("Trying to obtain non-reserved register \"") +
                          RegName + "\"; add -ffixed-" + RegName + ".");
  return Reg;
}

const char *AlphaTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<AlphaISD::NodeType>(Opcode)) {
  case AlphaISD::FIRST_NUMBER:
    break;
  case AlphaISD::RET_GLUE:
    return "AlphaISD::RET_GLUE";
  case AlphaISD::LITERAL:
    return "AlphaISD::LITERAL";
  case AlphaISD::CALL:
    return "AlphaISD::CALL";
  case AlphaISD::TC_RETURN:
    return "AlphaISD::TC_RETURN";
  case AlphaISD::TC_RETURN_BR:
    return "AlphaISD::TC_RETURN_BR";
  case AlphaISD::CALL_DIRECT:
    return "AlphaISD::CALL_DIRECT";
  case AlphaISD::CALL_DIRECT_LOCAL:
    return "AlphaISD::CALL_DIRECT_LOCAL";
  case AlphaISD::CALL_BSR:
    return "AlphaISD::CALL_BSR";
  case AlphaISD::CALL_TLSGD:
    return "AlphaISD::CALL_TLSGD";
  case AlphaISD::CALL_TLSLDM:
    return "AlphaISD::CALL_TLSLDM";
  case AlphaISD::DIVCALL:
    return "AlphaISD::DIVCALL";
  case AlphaISD::GPREL_HI:
    return "AlphaISD::GPREL_HI";
  case AlphaISD::GPREL_LO:
    return "AlphaISD::GPREL_LO";
  case AlphaISD::MOVI2F_S:
    return "AlphaISD::MOVI2F_S";
  case AlphaISD::MOVF2I_S:
    return "AlphaISD::MOVF2I_S";
  case AlphaISD::BR_EQ:
    return "AlphaISD::BR_EQ";
  case AlphaISD::BR_NE:
    return "AlphaISD::BR_NE";
  case AlphaISD::BR_LT:
    return "AlphaISD::BR_LT";
  case AlphaISD::BR_LE:
    return "AlphaISD::BR_LE";
  case AlphaISD::BR_GT:
    return "AlphaISD::BR_GT";
  case AlphaISD::BR_GE:
    return "AlphaISD::BR_GE";
  case AlphaISD::TPREL_HI:
    return "AlphaISD::TPREL_HI";
  case AlphaISD::TPREL_LO:
    return "AlphaISD::TPREL_LO";
  case AlphaISD::GOTTPREL:
    return "AlphaISD::GOTTPREL";
  case AlphaISD::TLSGD:
    return "AlphaISD::TLSGD";
  case AlphaISD::TLSLDM:
    return "AlphaISD::TLSLDM";
  case AlphaISD::DTPREL_HI:
    return "AlphaISD::DTPREL_HI";
  case AlphaISD::DTPREL_LO:
    return "AlphaISD::DTPREL_LO";
  case AlphaISD::SAFE_USTORE:
    return "AlphaISD::SAFE_USTORE";
  }
  return nullptr;
}

namespace {
// The extract/insert/mask intrinsics for a misaligned access of the given byte
// width (2, 4 or 8): the low form works on the first quadword, the high form on
// the second.
struct UnalignedOps {
  Intrinsic::ID ExtL, ExtH, InsL, InsH, MskL, MskH;
};
} // namespace

static UnalignedOps getUnalignedOps(unsigned Bytes) {
  switch (Bytes) {
  case 2:
    return {Intrinsic::alpha_extwl, Intrinsic::alpha_extwh,
            Intrinsic::alpha_inswl, Intrinsic::alpha_inswh,
            Intrinsic::alpha_mskwl, Intrinsic::alpha_mskwh};
  case 4:
    return {Intrinsic::alpha_extll, Intrinsic::alpha_extlh,
            Intrinsic::alpha_insll, Intrinsic::alpha_inslh,
            Intrinsic::alpha_mskll, Intrinsic::alpha_msklh};
  default: // 8
    return {Intrinsic::alpha_extql, Intrinsic::alpha_extqh,
            Intrinsic::alpha_insql, Intrinsic::alpha_insqh,
            Intrinsic::alpha_mskql, Intrinsic::alpha_mskqh};
  }
}

// Emit one of the extract/insert/mask byte intrinsics, op(Data, Ptr).
static SDValue emitByteOp(SelectionDAG &DAG, const SDLoc &dl, Intrinsic::ID Id,
                          SDValue Data, SDValue Ptr) {
  return DAG.getNode(ISD::INTRINSIC_WO_CHAIN, dl, MVT::i64,
                     DAG.getTargetConstant(Id, dl, MVT::i32), Data, Ptr);
}

// Load the aligned quadword containing an address with ldq_u.
static SDValue emitLdqU(SelectionDAG &DAG, const SDLoc &dl, SDValue Chain,
                        SDValue Ptr, MachineMemOperand::Flags Flags) {
  SDVTList VTs = DAG.getVTList(MVT::i64, MVT::Other);
  return DAG.getMemIntrinsicNode(AlphaISD::LDQ_U, dl, VTs, {Chain, Ptr},
                                 MVT::i64, MachinePointerInfo(), Align(8),
                                 Flags | MachineMemOperand::MOLoad);
}

SDValue AlphaTargetLowering::LowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  LoadSDNode *LD = cast<LoadSDNode>(Op);
  EVT MemVT = LD->getMemoryVT();
  unsigned Bytes = MemVT.getStoreSize();
  // A byte is inherently aligned; leave aligned accesses to the default path.
  if (Bytes < 2 || LD->getAlign().value() >= Bytes)
    return SDValue();

  SDLoc dl(Op);
  SDValue Chain = LD->getChain();
  SDValue Ptr = LD->getBasePtr();
  auto Flags = LD->isVolatile() ? MachineMemOperand::MOVolatile
                                : MachineMemOperand::MONone;
  SDValue PtrHi = DAG.getNode(ISD::ADD, dl, MVT::i64, Ptr,
                              DAG.getConstant(Bytes - 1, dl, MVT::i64));
  SDValue Lo = emitLdqU(DAG, dl, Chain, Ptr, Flags);
  SDValue Hi = emitLdqU(DAG, dl, Chain, PtrHi, Flags);
  UnalignedOps Ops = getUnalignedOps(Bytes);
  SDValue Val =
      DAG.getNode(ISD::OR, dl, MVT::i64, emitByteOp(DAG, dl, Ops.ExtL, Lo, Ptr),
                  emitByteOp(DAG, dl, Ops.ExtH, Hi, Ptr));
  // The extract zero-fills above the field, so sign-extend for a sextload.
  if (LD->getExtensionType() == ISD::SEXTLOAD)
    Val = DAG.getNode(ISD::SIGN_EXTEND_INREG, dl, MVT::i64, Val,
                      DAG.getValueType(MemVT));
  SDValue NewChain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other,
                                 Lo.getValue(1), Hi.getValue(1));
  return DAG.getMergeValues({Val, NewChain}, dl);
}

SDValue AlphaTargetLowering::LowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *ST = cast<StoreSDNode>(Op);
  EVT MemVT = ST->getMemoryVT();
  unsigned Bytes = MemVT.getStoreSize();
  if (Bytes < 2 || ST->getAlign().value() >= Bytes)
    return SDValue();

  SDLoc dl(Op);
  SDValue Chain = ST->getChain();
  SDValue Ptr = ST->getBasePtr();
  SDValue Val = ST->getValue();
  // With -msafe-partial, update each spanned quadword with a lock-based loop
  // (emitted by the custom inserter) so the read-modify-write is atomic.  The
  // node carries the store's memory operand, so it has to be built as a memory
  // node: the expansion reads it back off the node to put on the ldq_l and
  // stq_c it builds, and casting the result of getNode to MemSDNode is
  // undefined behaviour -- there is nothing behind it to read.
  if (Subtarget.hasSafePartial())
    return DAG.getMemIntrinsicNode(
        AlphaISD::SAFE_USTORE, dl, DAG.getVTList(MVT::Other),
        {Chain, Val, Ptr, DAG.getConstant(Bytes, dl, MVT::i64)}, MemVT,
        ST->getMemOperand());
  // A misaligned store reads the one or two quadwords the field falls in,
  // splices the field into them and writes them back.  That has to stay
  // indivisible -- one instruction, and then one bundle: two such stores can
  // fall in one quadword, and if one's reads are hoisted above the other's
  // write-backs the field written first is lost.  The node carries the store's
  // memory operand, so it has to be built as a memory node: instruction
  // selection reads it back off the node to give it to the instruction it
  // builds, and casting the result of getNode to MemSDNode is undefined
  // behaviour -- there is nothing behind it to read.
  return DAG.getMemIntrinsicNode(
      AlphaISD::USTORE, dl, DAG.getVTList(MVT::Other),
      {Chain, Val, Ptr, DAG.getConstant(Bytes, dl, MVT::i64)}, MemVT,
      ST->getMemOperand());
}

// f32 = bitcast i32.  The i32 source occupies the low 32 bits of an integer
// register; move them into an S_floating register (itofs, or a stack bounce).
SDValue AlphaTargetLowering::LowerBITCAST(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Src = Op.getOperand(0);
  if (Op.getValueType() != MVT::f32)
    return SDValue();
  if (Src.getValueType() != MVT::i64)
    Src = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, Src);
  return DAG.getNode(AlphaISD::MOVI2F_S, DL, MVT::f32, Src);
}

// i32 = bitcast f32 (i32 is illegal, so this comes through result
// legalization). Move the S_floating value into an integer register (ftois,
// sign-extended).
void AlphaTargetLowering::ReplaceNodeResults(SDNode *N,
                                             SmallVectorImpl<SDValue> &Results,
                                             SelectionDAG &DAG) const {
  SDLoc DL(N);
  if (N->getOpcode() == ISD::BITCAST && N->getValueType(0) == MVT::i32 &&
      N->getOperand(0).getValueType() == MVT::f32) {
    SDValue I64 =
        DAG.getNode(AlphaISD::MOVF2I_S, DL, MVT::i64, N->getOperand(0));
    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, I64));
  }
}

SDValue AlphaTargetLowering::LowerOperation(SDValue Op,
                                            SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::ATOMIC_FENCE:
    // A fence within a single thread orders nothing another processor can see:
    // it exists only to keep the compiler from moving accesses across it, and
    // MEMBARRIER says that without asking for an instruction.  A cross-thread
    // one falls through to the mb pattern.
    if (static_cast<SyncScope::ID>(Op.getConstantOperandVal(2)) ==
        SyncScope::SingleThread)
      return DAG.getNode(ISD::MEMBARRIER, SDLoc(Op), MVT::Other,
                         Op.getOperand(0));
    return Op;
  case ISD::GlobalAddress:
    return LowerGlobalAddress(Op, DAG);
  case ISD::GlobalTLSAddress:
    return LowerGlobalTLSAddress(Op, DAG);
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return LowerJumpTable(Op, DAG);
  case ISD::BlockAddress:
    return LowerBlockAddress(Op, DAG);
  case ISD::RETURNADDR:
    return LowerRETURNADDR(Op, DAG);
  case ISD::FRAMEADDR:
    return LowerFRAMEADDR(Op, DAG);
  case ISD::DYNAMIC_STACKALLOC:
    return LowerDYNAMIC_STACKALLOC(Op, DAG);
  case ISD::BR_CC:
    return LowerBR_CC(Op, DAG);
  case ISD::BR_JT:
    return LowerBR_JT(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAARG:
    return LowerVAARG(Op, DAG);
  case ISD::VACOPY:
    return LowerVACOPY(Op, DAG);
  case ISD::BITCAST:
    return LowerBITCAST(Op, DAG);
  case ISD::MULHS:
    return LowerMULHS(Op, DAG);
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::SREM:
  case ISD::UREM:
    return LowerDivRem(Op, DAG);
  case ISD::PREFETCH:
    // Available only on the 21264 and later; drop it elsewhere (a load to
    // R31/F31 there is an ordinary load that can fault).  When available, keep
    // the node for the prefetch patterns to select.
    if (!Subtarget.hasPrefetch())
      return Op.getOperand(0);
    return Op;
  case ISD::LOAD:
    return LowerLOAD(Op, DAG);
  case ISD::STORE:
    return LowerSTORE(Op, DAG);
  default:
    llvm_unreachable("unexpected operation to lower");
  }
}

SDValue AlphaTargetLowering::LowerMULHS(SDValue Op, SelectionDAG &DAG) const {
  // umulh gives the high half of the unsigned product; correct it to the signed
  // high half:  mulhs(a,b) = umulh(a,b) - (a<0 ? b : 0) - (b<0 ? a : 0).
  // sra by 63 yields all-ones for a negative operand and zero otherwise, so the
  // masked term is b (resp. a) exactly when a (resp. b) is negative.  When b is
  // a positive constant (the magic-number division case) the second term folds
  // away entirely.
  SDLoc DL(Op);
  EVT VT = MVT::i64;
  SDValue A = Op.getOperand(0);
  SDValue B = Op.getOperand(1);
  SDValue Sh = DAG.getConstant(63, DL, VT);
  SDValue Hi = DAG.getNode(ISD::MULHU, DL, VT, A, B);
  SDValue TA =
      DAG.getNode(ISD::AND, DL, VT, DAG.getNode(ISD::SRA, DL, VT, A, Sh), B);
  SDValue TB =
      DAG.getNode(ISD::AND, DL, VT, DAG.getNode(ISD::SRA, DL, VT, B, Sh), A);
  Hi = DAG.getNode(ISD::SUB, DL, VT, Hi, TA);
  return DAG.getNode(ISD::SUB, DL, VT, Hi, TB);
}

SDValue AlphaTargetLowering::LowerDivRem(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  MachineFunction &MF = DAG.getMachineFunction();
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  const char *Name;
  switch (Op.getOpcode()) {
  case ISD::SDIV:
    Name = "__divq";
    break;
  case ISD::UDIV:
    Name = "__divqu";
    break;
  case ISD::SREM:
    Name = "__remq";
    break;
  case ISD::UREM:
    Name = "__remqu";
    break;
  default:
    llvm_unreachable("unexpected division");
  }

  // Millicode convention: dividend in $24, divisor in $25, entered through $23
  // with the routine address in $27, result in $27.
  SDValue Chain = DAG.getEntryNode();
  SDValue Glue;
  Chain = DAG.getCopyToReg(Chain, DL, Alpha::R24, Op.getOperand(0), Glue);
  Glue = Chain.getValue(1);
  Chain = DAG.getCopyToReg(Chain, DL, Alpha::R25, Op.getOperand(1), Glue);
  Glue = Chain.getValue(1);
  SDValue Pv = DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64,
                           DAG.getTargetExternalSymbol(Name, MVT::i64));
  Chain = DAG.getCopyToReg(Chain, DL, Alpha::R27, Pv, Glue);
  Glue = Chain.getValue(1);

  SDValue Ops[] = {Chain, DAG.getRegister(Alpha::R24, MVT::i64),
                   DAG.getRegister(Alpha::R25, MVT::i64),
                   DAG.getRegister(Alpha::R27, MVT::i64), Glue};
  Chain = DAG.getNode(AlphaISD::DIVCALL, DL, {MVT::Other, MVT::Glue}, Ops);
  Glue = Chain.getValue(1);

  return DAG.getCopyFromReg(Chain, DL, Alpha::R27, MVT::i64, Glue);
}

// Whether a global's address can be computed from the global pointer instead
// of being loaded from the GOT.  A gp-relative address is an ldah/lda pair
// covering a signed 32-bit displacement, which reaches anywhere in the data
// segment, so the only question is whether the linker will let this reference
// see the definition's own address.  This mirrors gcc's local_symbolic_operand.
static bool isGprelAddressable(const GlobalValue &GV) {
  // A preemptible symbol's address is whatever the dynamic linker picks.
  if (!GV.isDSOLocal())
    return false;
  // An undefined weak symbol has to read as zero, and an ifunc's address is the
  // one its resolver returned; both of those the GOT entry supplies.
  if (GV.hasExternalWeakLinkage() || isa<GlobalIFunc>(GV))
    return false;
  // An absolute symbol is not in the data segment and has no gp offset.
  if (const auto *Var = dyn_cast<GlobalVariable>(&GV))
    if (Var->isAbsoluteSymbolRef())
      return false;
  return true;
}

SDValue AlphaTargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  // Loading a global's address establishes and uses the global pointer.
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  auto *N = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  SDValue TGA =
      DAG.getTargetGlobalAddress(N->getGlobal(), DL, MVT::i64, N->getOffset());

  // A global the linker resolves itself is a fixed distance from the global
  // pointer, so form the address GP-relative (ldah/lda !gprelhigh/!gprellow)
  // rather than loading it from the GOT.  Only a symbol the GOT entry itself
  // has to answer for stays there.  This is not just a saving of one
  // instruction: a gp reaches 64KB of GOT, i.e. 8192 entries, and spending one
  // on every local string constant overflows that on a large link.  Being in a
  // small section is about where the object is placed, not about whether its
  // address can be formed from gp: a preemptible definition still goes in
  // .sdata under -msmall-data, but its address is whatever the dynamic linker
  // picks, so it is reached through the GOT -- gcc does exactly this, emitting
  // `.sbss' placement together with an `!literal' load for a
  // default-visibility global built -fPIC -msmall-data.
  if (isGprelAddressable(*N->getGlobal())) {
    SDValue GP = DAG.getRegister(Alpha::R29, MVT::i64);
    SDValue Hi = DAG.getNode(AlphaISD::GPREL_HI, DL, MVT::i64, TGA, GP);
    return DAG.getNode(AlphaISD::GPREL_LO, DL, MVT::i64, TGA, Hi);
  }
  return DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64, TGA);
}

SDValue AlphaTargetLowering::LowerGlobalTLSAddress(SDValue Op,
                                                   SelectionDAG &DAG) const {
  auto *N = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  const GlobalValue *GV = N->getGlobal();
  TLSModel::Model Model = getTargetMachine().getTLSModel(GV);

  SDValue TGA = DAG.getTargetGlobalAddress(GV, DL, MVT::i64, N->getOffset());

  if (Model == TLSModel::GeneralDynamic || Model == TLSModel::LocalDynamic) {
    // Pass a TLS descriptor in $16 to __tls_get_addr.  General-dynamic passes
    // the tlsgd descriptor and gets the variable's address back in $0;
    // local-dynamic passes the tlsldm descriptor and gets the module's TLS
    // base, to which the variable's dtprel offset is then added.
    bool IsGD = Model == TLSModel::GeneralDynamic;
    MachineFunction &MF = DAG.getMachineFunction();
    MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

    SDValue Arg = DAG.getNode(IsGD ? AlphaISD::TLSGD : AlphaISD::TLSLDM, DL,
                              MVT::i64, TGA);
    SDValue Callee =
        DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64,
                    DAG.getTargetExternalSymbol("__tls_get_addr", MVT::i64));

    SDValue Chain = DAG.getCALLSEQ_START(DAG.getEntryNode(), 0, 0, DL);
    SDValue Glue;
    Chain = DAG.getCopyToReg(Chain, DL, Alpha::R16, Arg, Glue);
    Glue = Chain.getValue(1);
    Chain = DAG.getCopyToReg(Chain, DL, Alpha::R27, Callee, Glue);
    Glue = Chain.getValue(1);

    const uint32_t *Mask =
        Subtarget.getRegisterInfo()->getCallPreservedMask(MF, CallingConv::C);
    SDValue Ops[] = {Chain, DAG.getRegister(Alpha::R16, MVT::i64),
                     DAG.getRegister(Alpha::R27, MVT::i64),
                     DAG.getRegisterMask(Mask), Glue};
    Chain = DAG.getNode(IsGD ? AlphaISD::CALL_TLSGD : AlphaISD::CALL_TLSLDM, DL,
                        {MVT::Other, MVT::Glue}, Ops);
    Glue = Chain.getValue(1);
    Chain = DAG.getCALLSEQ_END(Chain, 0, 0, Glue, DL);
    Glue = Chain.getValue(1);
    SDValue Ret = DAG.getCopyFromReg(Chain, DL, Alpha::R0, MVT::i64, Glue);

    if (IsGD)
      return Ret;

    // Local-dynamic: add the variable's module-relative offset to the base.
    SDValue Hi = DAG.getNode(AlphaISD::DTPREL_HI, DL, MVT::i64, TGA, Ret);
    return DAG.getNode(AlphaISD::DTPREL_LO, DL, MVT::i64, TGA, Hi);
  }

  // The two models left both start from the thread pointer, read from the
  // PALcode unique value (call_pal rduniq).  The result is fixed in $0, so glue
  // the copy to the call.  The models above do not come this way: they get the
  // address from __tls_get_addr, which returns in that same register.
  SDValue RdUniq = SDValue(DAG.getMachineNode(Alpha::RDUNIQ, DL, MVT::Glue), 0);
  SDValue TP =
      DAG.getCopyFromReg(DAG.getEntryNode(), DL, Alpha::R0, MVT::i64, RdUniq);

  if (Model == TLSModel::InitialExec) {
    // The offset from the thread pointer is loaded from the GOT
    // (ldq !gottprel), then added to the thread pointer.
    DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();
    SDValue Off = DAG.getNode(AlphaISD::GOTTPREL, DL, MVT::i64, TGA);
    return DAG.getNode(ISD::ADD, DL, MVT::i64, TP, Off);
  }

  // Local-exec: the offset from the thread pointer is a link-time constant
  // formed with ldah !tprelhi / lda !tprello.
  assert(Model == TLSModel::LocalExec && "unexpected TLS model");

  SDValue Hi = DAG.getNode(AlphaISD::TPREL_HI, DL, MVT::i64, TGA, TP);
  return DAG.getNode(AlphaISD::TPREL_LO, DL, MVT::i64, TGA, Hi);
}

SDValue AlphaTargetLowering::LowerConstantPool(SDValue Op,
                                               SelectionDAG &DAG) const {
  // Constant-pool entries are local, so their address is formed GP-relative:
  // ldah $r, cp($gp) !gprelhigh ; lda $dst, cp($r) !gprellow.
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  auto *CP = cast<ConstantPoolSDNode>(Op);
  SDLoc DL(Op);
  SDValue TCP = DAG.getTargetConstantPool(CP->getConstVal(), MVT::i64,
                                          CP->getAlign(), CP->getOffset());
  SDValue GP = DAG.getRegister(Alpha::R29, MVT::i64);
  SDValue Hi = DAG.getNode(AlphaISD::GPREL_HI, DL, MVT::i64, TCP, GP);
  return DAG.getNode(AlphaISD::GPREL_LO, DL, MVT::i64, TCP, Hi);
}

SDValue AlphaTargetLowering::LowerJumpTable(SDValue Op,
                                            SelectionDAG &DAG) const {
  // Like a constant pool, the jump table is addressed GP-relative.
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  auto *JT = cast<JumpTableSDNode>(Op);
  SDLoc DL(Op);
  SDValue TJT = DAG.getTargetJumpTable(JT->getIndex(), MVT::i64);
  SDValue GP = DAG.getRegister(Alpha::R29, MVT::i64);
  SDValue Hi = DAG.getNode(AlphaISD::GPREL_HI, DL, MVT::i64, TJT, GP);
  return DAG.getNode(AlphaISD::GPREL_LO, DL, MVT::i64, TJT, Hi);
}

SDValue AlphaTargetLowering::LowerRETURNADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  MF.getFrameInfo().setReturnAddressIsTaken(true);

  SDLoc DL(Op);
  // Only the current frame's return address is available; deeper frames are not
  // reachable without unwinding, so return zero for them.
  if (Op.getConstantOperandVal(0) != 0)
    return DAG.getConstant(0, DL, MVT::i64);

  // The return address arrives in $26; capture its entry value.
  Register RA = MF.addLiveIn(Alpha::R26, &Alpha::GPRCRegClass);
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, RA, MVT::i64);
}

SDValue AlphaTargetLowering::LowerFRAMEADDR(SDValue Op,
                                            SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  SDLoc DL(Op);

  // A frame carries no link to the one that called it, so only this function's
  // own frame can be named; deeper frames would need an unwinder.  Answering
  // for one of those needs no frame of our own, so say nothing was taken.
  if (Op.getConstantOperandVal(0) != 0)
    return DAG.getConstant(0, DL, MVT::i64);

  MF.getFrameInfo().setFrameAddressIsTaken(true);

  // Taking the frame address forces a frame pointer (see hasFPImpl), so this
  // reads $15 as the prologue set it up, not the value it arrived with.
  Register FP = Subtarget.getRegisterInfo()->getFrameRegister(MF);
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, FP, MVT::i64);
}

SDValue AlphaTargetLowering::LowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  // A block address (taken with &&label) is local, so it is addressed
  // GP-relative just like a jump table or constant pool entry.
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  auto *BA = cast<BlockAddressSDNode>(Op);
  SDLoc DL(Op);
  SDValue TBA = DAG.getTargetBlockAddress(BA->getBlockAddress(), MVT::i64,
                                          BA->getOffset());
  SDValue GP = DAG.getRegister(Alpha::R29, MVT::i64);
  SDValue Hi = DAG.getNode(AlphaISD::GPREL_HI, DL, MVT::i64, TBA, GP);
  return DAG.getNode(AlphaISD::GPREL_LO, DL, MVT::i64, TBA, Hi);
}

SDValue AlphaTargetLowering::LowerDYNAMIC_STACKALLOC(SDValue Op,
                                                     SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue Size = Op.getOperand(1);
  MaybeAlign Alignment =
      cast<ConstantSDNode>(Op.getOperand(2))->getMaybeAlignValue();
  SDLoc DL(Op);

  // Round the requested size up to the 16-byte stack alignment and subtract it
  // from the stack pointer.
  Size = DAG.getNode(ISD::ADD, DL, MVT::i64, Size,
                     DAG.getConstant(15, DL, MVT::i64));
  Size = DAG.getNode(ISD::AND, DL, MVT::i64, Size,
                     DAG.getSignedConstant(-16, DL, MVT::i64));

  SDValue SP = DAG.getCopyFromReg(Chain, DL, Alpha::R30, MVT::i64);
  SDValue NewSP = DAG.getNode(ISD::SUB, DL, MVT::i64, SP, Size);

  // Over-align the result if the allocation needs more than the stack default.
  if (Alignment && *Alignment > Align(16))
    NewSP = DAG.getNode(
        ISD::AND, DL, MVT::i64, NewSP,
        DAG.getSignedConstant(-(int64_t)Alignment->value(), DL, MVT::i64));

  Chain = DAG.getCopyToReg(SP.getValue(1), DL, Alpha::R30, NewSP);
  return DAG.getMergeValues({NewSP, Chain}, DL);
}

SDValue AlphaTargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  ISD::CondCode CC = cast<CondCodeSDNode>(Op.getOperand(1))->get();
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);
  SDValue Dest = Op.getOperand(4);
  SDLoc DL(Op);

  // A signed comparison whose branch reduces to testing LHS against zero is a
  // single test-and-branch.  The middle end canonicalizes the "or equal"
  // relations against zero to a strict comparison against +/-1 (x >= 0 becomes
  // x > -1, x > 0 becomes x >= 1, and so on), so recognize those forms too.
  if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
    int64_t V = C->getSExtValue();
    unsigned Opc = 0;
    if (V == 0) {
      switch (CC) {
      case ISD::SETEQ:
        Opc = AlphaISD::BR_EQ;
        break;
      case ISD::SETNE:
        Opc = AlphaISD::BR_NE;
        break;
      case ISD::SETLT:
        Opc = AlphaISD::BR_LT;
        break;
      case ISD::SETLE:
        Opc = AlphaISD::BR_LE;
        break;
      case ISD::SETGT:
        Opc = AlphaISD::BR_GT;
        break;
      case ISD::SETGE:
        Opc = AlphaISD::BR_GE;
        break;
      default:
        break;
      }
    } else if (V == 1) {
      if (CC == ISD::SETGE) // x >= 1  <=>  x > 0
        Opc = AlphaISD::BR_GT;
      else if (CC == ISD::SETLT) // x < 1   <=>  x <= 0
        Opc = AlphaISD::BR_LE;
    } else if (V == -1) {
      if (CC == ISD::SETGT) // x > -1  <=>  x >= 0
        Opc = AlphaISD::BR_GE;
      else if (CC == ISD::SETLE) // x <= -1 <=>  x < 0
        Opc = AlphaISD::BR_LT;
    }
    if (Opc)
      return DAG.getNode(Opc, DL, MVT::Other, Chain, LHS, Dest);
  }

  // Otherwise compute the 0/1 comparison result and branch if it is nonzero.
  // Emit the branch as a target node rather than ISD::BRCOND: a BRCOND of a
  // SETCC would be folded straight back into BR_CC and lowered again forever.
  SDValue Cond = DAG.getSetCC(DL, MVT::i64, LHS, RHS, CC);
  return DAG.getNode(AlphaISD::BR_NE, DL, MVT::Other, Chain, Cond, Dest);
}

SDValue AlphaTargetLowering::LowerBR_JT(SDValue Op, SelectionDAG &DAG) const {
  // br_jt: chain, jump-table address, index.
  SDValue Chain = Op.getOperand(0);
  SDValue Index = Op.getOperand(2);
  SDLoc DL(Op);
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  // Form the GP-relative address of the table.
  auto *JT = cast<JumpTableSDNode>(Op.getOperand(1));
  SDValue TJT = DAG.getTargetJumpTable(JT->getIndex(), MVT::i64);
  SDValue GPReg = DAG.getRegister(Alpha::R29, MVT::i64);
  SDValue Hi = DAG.getNode(AlphaISD::GPREL_HI, DL, MVT::i64, TJT, GPReg);
  SDValue Table = DAG.getNode(AlphaISD::GPREL_LO, DL, MVT::i64, TJT, Hi);

  // Each entry is an absolute 64-bit block address; load it and jump.
  SDValue EntryAddr =
      DAG.getNode(ISD::ADD, DL, MVT::i64, Table,
                  DAG.getNode(ISD::SHL, DL, MVT::i64, Index,
                              DAG.getConstant(3, DL, MVT::i64)));
  SDValue Target =
      DAG.getLoad(MVT::i64, DL, Chain, EntryAddr,
                  MachinePointerInfo::getJumpTable(DAG.getMachineFunction()));
  Chain = Target.getValue(1);
  return DAG.getNode(ISD::BRIND, DL, MVT::Other, Chain, Target);
}

bool AlphaTargetLowering::isEligibleForTailCallOptimization(
    CallingConv::ID CallerCC, CallingConv::ID CalleeCC, bool IsVarArg,
    unsigned NumStackBytes, const SmallVectorImpl<ISD::OutputArg> &Outs) const {
  // The tail-callee returns straight to our caller, so it must return the way
  // this function would have: the same convention, and one of the two whose
  // return lowering this backend implements.
  if (CallerCC != CalleeCC)
    return false;
  if (CalleeCC != CallingConv::C && CalleeCC != CallingConv::Fast)
    return false;
  // A variadic callee, or one that needs outgoing stack arguments, would need
  // stack space that overlaps the frame we are tearing down.
  if (IsVarArg || NumStackBytes != 0)
    return false;
  // A byval argument is a copy the caller made in its own frame and passes by
  // address.  The epilogue runs before the jump, so that copy is below the
  // stack pointer by the time the callee reads it.  (This is how a long double
  // argument is passed, so it is not a rare case.)
  for (const ISD::OutputArg &Out : Outs)
    if (Out.Flags.isByVal())
      return false;
  return true;
}

SDValue AlphaTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  MachineFunction &MF = DAG.getMachineFunction();

  // The caller uses the global pointer to load the callee address and, for a
  // non-tail call, to reload gp afterwards.  Under -msmall-text there is no
  // such load, but the caller still needs a correct gp: the branch lands past
  // the callee's own prologue and the callee runs on ours.
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(CLI.Outs, CC_Alpha);
  unsigned NumBytes = CCInfo.getStackSize();

  // A call in tail position is turned into a jump through $27 when it passes
  // nothing on the stack (so it reuses the caller's frame) and shares the
  // caller's calling convention.
  bool IsTailCall =
      CLI.IsTailCall && isEligibleForTailCallOptimization(
                            MF.getFunction().getCallingConv(), CLI.CallConv,
                            CLI.IsVarArg, NumBytes, CLI.Outs);
  CLI.IsTailCall = IsTailCall;

  SDValue Chain = CLI.Chain;
  if (!IsTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, DL);

  // Copy each argument into its assigned register or store it to the outgoing
  // argument area.  The callee address goes in $27 (the procedure value).
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOps;
  SDValue StackPtr;
  for (unsigned I = 0, E = ArgLocs.size(); I != E; ++I) {
    const CCValAssign &VA = ArgLocs[I];
    if (VA.isRegLoc()) {
      RegsToPass.emplace_back(VA.getLocReg(), CLI.OutVals[I]);
      continue;
    }
    if (!StackPtr.getNode())
      StackPtr = DAG.getRegister(Alpha::R30, MVT::i64);
    SDValue Off = DAG.getNode(ISD::ADD, DL, MVT::i64, StackPtr,
                              DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));
    MemOps.push_back(
        DAG.getStore(Chain, DL, CLI.OutVals[I], Off,
                     MachinePointerInfo::getStack(MF, VA.getLocMemOffset())));
  }
  if (!MemOps.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOps);

  // For a direct call the callee address is loaded from the GOT; remember the
  // symbol so the jsr can carry its hint and lituse_jsr relocations.  A
  // dso-local callee can be relaxed by the linker, so it takes only the
  // lituse_jsr relocation (a hint on the jsr would inhibit that relaxation).
  // Under -msmall-text a direct call to a callee the linker resolves itself is
  // a single bsr to the symbol, so it needs neither the procedure value loaded
  // into $27 nor a global-pointer reload.  A preemptible callee still goes
  // through the GOT: its address is the dynamic linker's to choose, and there
  // is no pc-relative relocation against a dynamic symbol to name it with.
  bool BsrCall = false;
  SDValue Callee = CLI.Callee;
  SDValue TargetSym;
  bool IsLocal = false;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee)) {
    TargetSym = DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i64);
    IsLocal = G->getGlobal()->isDSOLocal();
    BsrCall = Subtarget.hasSmallText() && IsLocal;
    if (!BsrCall)
      Callee = DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64, TargetSym);
  } else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    // A runtime-library callee has no GlobalValue to ask about preemption, so
    // take the GOT: memcpy and friends are commonly the shared libc's.
    TargetSym = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i64);
    Callee = DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64, TargetSym);
  }
  // Otherwise the callee address is already a value; use it directly as the
  // procedure value for an indirect call.  A bsr call needs no procedure value.
  if (!BsrCall)
    RegsToPass.emplace_back(Alpha::R27, Callee);

  SDValue Glue;
  for (auto &R : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, R.first, R.second, Glue);
    Glue = Chain.getValue(1);
  }

  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CLI.CallConv);

  // A tail call jumps through the procedure value in $27 (already copied
  // above), with the argument registers held live by the glue chain.  The
  // epilogue is inserted before this terminator, so the frame is gone by the
  // time we jump. No register mask is attached: nothing is live past the jump,
  // and jmp (unlike jsr) leaves $26 untouched, so a function whose only call is
  // a tail call does not clobber the return address and needs no frame to
  // preserve it. Under -msmall-text there is no procedure value at all: the
  // callee is in range of a PC-relative branch and shares the global pointer,
  // so the jump is that branch and $27 is never written.
  if (IsTailCall) {
    SmallVector<SDValue, 8> Ops(1, Chain);
    if (BsrCall)
      Ops.push_back(TargetSym);
    for (auto &R : RegsToPass)
      Ops.push_back(DAG.getRegister(R.first, R.second.getValueType()));
    if (Glue.getNode())
      Ops.push_back(Glue);
    return DAG.getNode(BsrCall ? AlphaISD::TC_RETURN_BR : AlphaISD::TC_RETURN,
                       DL, MVT::Other, Ops);
  }

  // A direct external call passes the callee symbol as the first operand so the
  // jsr is tagged with the hint and lituse_jsr relocations; a direct local call
  // carries only lituse_jsr; an indirect call has neither.
  bool IsDirect = TargetSym.getNode() != nullptr;
  bool WithHint = IsDirect && !IsLocal && !BsrCall;
  SmallVector<SDValue, 8> Ops(1, Chain);
  if (WithHint || BsrCall)
    Ops.push_back(TargetSym);
  for (auto &R : RegsToPass)
    Ops.push_back(DAG.getRegister(R.first, R.second.getValueType()));

  Ops.push_back(DAG.getRegisterMask(Mask));
  if (Glue.getNode())
    Ops.push_back(Glue);

  unsigned CallOpc = AlphaISD::CALL;
  if (BsrCall)
    CallOpc = AlphaISD::CALL_BSR;
  else if (WithHint)
    CallOpc = AlphaISD::CALL_DIRECT;
  else if (IsDirect)
    CallOpc = AlphaISD::CALL_DIRECT_LOCAL;
  Chain = DAG.getNode(CallOpc, DL, {MVT::Other, MVT::Glue}, Ops);
  Glue = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  // Copy the return values out of $0 / $f0.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CLI.CallConv, CLI.IsVarArg, MF, RVLocs, *DAG.getContext());
  RetCCInfo.AnalyzeCallResult(CLI.Ins, RetCC_Alpha);
  for (const CCValAssign &VA : RVLocs) {
    SDValue Val =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    Chain = Val.getValue(1);
    Glue = Val.getValue(2);
    InVals.push_back(Val);
  }

  return Chain;
}

static const TargetRegisterClass *getRegClassForVT(MVT VT) {
  if (VT == MVT::i64)
    return &Alpha::GPRCRegClass;
  if (VT == MVT::f32 || VT == MVT::f64)
    return &Alpha::FPRCRegClass;
  llvm_unreachable("Alpha lowering: unexpected value type");
}

SDValue AlphaTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_Alpha);

  MachineFrameInfo &MFI = MF.getFrameInfo();
  for (const CCValAssign &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      const TargetRegisterClass *RC = getRegClassForVT(VA.getLocVT());
      Register VReg = MF.addLiveIn(VA.getLocReg(), RC);
      SDValue Val = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
      InVals.push_back(Val);
    } else {
      // The argument was passed on the stack, in the caller's outgoing area.
      unsigned Size = VA.getLocVT().getStoreSize();
      int FI = MFI.CreateFixedObject(Size, VA.getLocMemOffset(),
                                     /*IsImmutable=*/true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i64);
      SDValue Val = DAG.getLoad(VA.getLocVT(), DL, Chain, FIN,
                                MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Val);
    }
  }

  // Keep the hidden result pointer of a function returning in memory: it is
  // returned again in $0 (see LowerReturn).
  assert((Ins.empty() || llvm::none_of(llvm::drop_begin(Ins),
                                       [](const ISD::InputArg &A) {
                                         return A.Flags.isSRet();
                                       })) &&
         "sret is the first argument");
  if (!Ins.empty() && Ins[0].Flags.isSRet()) {
    auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
    Register Reg = MF.getRegInfo().createVirtualRegister(&Alpha::GPRCRegClass);
    FI->setSRetReturnReg(Reg);
    Chain = DAG.getCopyToReg(Chain, DL, Reg, InVals[0]);
  }

  if (IsVarArg) {
    // Save the unnamed argument registers to a save area so va_arg can reach
    // them.  Layout (from the base): integer registers at [base, base+48),
    // floating-point registers at [base-48, base); stack arguments follow at
    // base+48.  The slot index is shared between the two areas.
    static const MCPhysReg IntArgRegs[] = {Alpha::R16, Alpha::R17, Alpha::R18,
                                           Alpha::R19, Alpha::R20, Alpha::R21};
    static const MCPhysReg FPArgRegs[] = {Alpha::F16, Alpha::F17, Alpha::F18,
                                          Alpha::F19, Alpha::F20, Alpha::F21};
    // Slot N of the argument list lives at IntBase + N*8 for every N: the six
    // register slots are the save area itself, and slot 6 onwards are the
    // caller's stack arguments, which start at the incoming stack pointer.  So
    // the integer save area must sit immediately below the incoming stack
    // pointer, at a fixed -48, with the floating-point area below it at -96.
    // These offsets do not depend on how much stack the named arguments used.
    unsigned NumNamed = CCInfo.getStackSize() / 8;
    for (const CCValAssign &VA : ArgLocs)
      if (VA.isRegLoc())
        ++NumNamed;
    int IntFI = MFI.CreateFixedObject(48, -48, /*IsImmutable=*/false);
    int FpFI = MFI.CreateFixedObject(48, -96, /*IsImmutable=*/false);
    SDValue IntBase = DAG.getFrameIndex(IntFI, MVT::i64);
    SDValue FpBase = DAG.getFrameIndex(FpFI, MVT::i64);

    SmallVector<SDValue, 12> Stores;
    for (unsigned I = NumNamed; I < 6; ++I) {
      Register IntReg = MF.addLiveIn(IntArgRegs[I], &Alpha::GPRCRegClass);
      SDValue IntVal = DAG.getCopyFromReg(Chain, DL, IntReg, MVT::i64);
      SDValue IntPtr =
          DAG.getMemBasePlusOffset(IntBase, TypeSize::getFixed(I * 8), DL);
      Stores.push_back(
          DAG.getStore(Chain, DL, IntVal, IntPtr,
                       MachinePointerInfo::getFixedStack(MF, IntFI, I * 8)));
      // With -mno-fp-regs there are no floating-point argument registers to
      // read: the file is out of the register classes altogether, so f64 is an
      // illegal type and a copy out of $f16 could not even be legalized.  A
      // floating-point argument arrives in an integer register under that flag,
      // so fill the floating-point save area from the integer registers too.
      // gcc does the same thing in alpha_setup_incoming_varargs, where the
      // register number the save area is filled from is
      // `16 + cum + TARGET_FPREGS*32' -- the integer one when TARGET_FPREGS is
      // zero.
      SDValue FPVal = IntVal;
      if (!Subtarget.hasNoFPRegs()) {
        Register FPReg = MF.addLiveIn(FPArgRegs[I], &Alpha::FPRCRegClass);
        FPVal = DAG.getCopyFromReg(Chain, DL, FPReg, MVT::f64);
      }
      SDValue FPPtr =
          DAG.getMemBasePlusOffset(FpBase, TypeSize::getFixed(I * 8), DL);
      Stores.push_back(
          DAG.getStore(Chain, DL, FPVal, FPPtr,
                       MachinePointerInfo::getFixedStack(MF, FpFI, I * 8)));
    }
    if (!Stores.empty())
      Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Stores);

    auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
    FI->setVarArgsFrameIndex(IntFI);
    FI->setVarArgsOffset(NumNamed * 8);
  }

  // The caller passes the return address in $26, which the return instruction
  // uses (directly for a leaf, or after the epilogue reloads it).  Mark it
  // live-in so that use has a definition reaching it from function entry.
  MF.getRegInfo().addLiveIn(Alpha::R26);

  return Chain;
}

Instruction *AlphaTargetLowering::emitLeadingFence(IRBuilderBase &Builder,
                                                   Instruction *Inst,
                                                   AtomicOrdering Ord) const {
  // A sequentially consistent access needs the barrier ahead of it whether or
  // not it stores, so that an earlier SC store cannot be reordered past a
  // later SC load.  The generic implementation asks for hasAtomicStore() here,
  // which is enough for release semantics but not for SC.
  if (Ord == AtomicOrdering::SequentiallyConsistent)
    return Builder.CreateFence(Ord);
  if (isReleaseOrStronger(Ord) && Inst->hasAtomicStore())
    return Builder.CreateFence(AtomicOrdering::Release);
  return nullptr;
}

Instruction *AlphaTargetLowering::emitTrailingFence(IRBuilderBase &Builder,
                                                    Instruction *Inst,
                                                    AtomicOrdering Ord) const {
  // Alpha has one barrier, so an acquire fence after the access is both what
  // orders the following accesses and what breaks the dependent-load problem
  // shouldInsertFencesForAtomic describes.
  if (isAcquireOrStronger(Ord))
    return Builder.CreateFence(AtomicOrdering::Acquire);
  return nullptr;
}

bool AlphaTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_Alpha);
}

SDValue AlphaTargetLowering::LowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  // Store the va_list { base, offset } that describes the argument save area.
  MachineFunction &MF = DAG.getMachineFunction();
  auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
  SDLoc DL(Op);

  SDValue Base = DAG.getFrameIndex(FI->getVarArgsFrameIndex(), MVT::i64);
  SDValue Chain = Op.getOperand(0);
  SDValue VAList = Op.getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();

  Chain = DAG.getStore(Chain, DL, Base, VAList, MachinePointerInfo(SV));
  // va_list[8] = offset, which is an int: gcc's alpha_build_builtin_va_list
  // gives the field integer_type_node, so writing a quadword here would
  // scribble on whatever the ABI puts in the tail padding.
  SDValue OffPtr = DAG.getMemBasePlusOffset(VAList, TypeSize::getFixed(8), DL);
  SDValue Off = DAG.getConstant(FI->getVarArgsOffset(), DL, MVT::i64);
  return DAG.getTruncStore(Chain, DL, Off, OffPtr, MachinePointerInfo(SV, 8),
                           MVT::i32);
}

SDValue AlphaTargetLowering::LowerVACOPY(SDValue Op, SelectionDAG &DAG) const {
  // The va_list is a { char *base; int offset; } pair, so both fields must be
  // copied; the default expansion copies only a single pointer.
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue DstPtr = Op.getOperand(1);
  SDValue SrcPtr = Op.getOperand(2);
  const Value *DstSV = cast<SrcValueSDNode>(Op.getOperand(3))->getValue();
  const Value *SrcSV = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();

  SDValue Base =
      DAG.getLoad(MVT::i64, DL, Chain, SrcPtr, MachinePointerInfo(SrcSV));
  Chain = Base.getValue(1);
  SDValue SrcOff = DAG.getMemBasePlusOffset(SrcPtr, TypeSize::getFixed(8), DL);
  SDValue Off = DAG.getExtLoad(ISD::ZEXTLOAD, DL, MVT::i64, Chain, SrcOff,
                               MachinePointerInfo(SrcSV, 8), MVT::i32);
  Chain = Off.getValue(1);

  Chain = DAG.getStore(Chain, DL, Base, DstPtr, MachinePointerInfo(DstSV));
  SDValue DstOff = DAG.getMemBasePlusOffset(DstPtr, TypeSize::getFixed(8), DL);
  return DAG.getTruncStore(Chain, DL, Off, DstOff, MachinePointerInfo(DstSV, 8),
                           MVT::i32);
}

SDValue AlphaTargetLowering::LowerVAARG(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Chain = Op.getOperand(0);
  SDValue VAList = Op.getOperand(1);
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  EVT VT = Op.getValueType();

  SDValue Base =
      DAG.getLoad(MVT::i64, DL, Chain, VAList, MachinePointerInfo(SV));
  Chain = Base.getValue(1);
  // __offset is an int, so this is a four-byte access widened into a register;
  // i32 is not a legal type here.  It is widened with a zero extension rather
  // than a sign extension: the offset starts at the named arguments' size and
  // only grows, so it is never negative, and the two agree on every value it
  // can hold.  Zero-extending lets a va_arg that follows its own va_start fold
  // the offset into the load's displacement.
  SDValue OffPtr = DAG.getMemBasePlusOffset(VAList, TypeSize::getFixed(8), DL);
  SDValue Offset = DAG.getExtLoad(ISD::ZEXTLOAD, DL, MVT::i64, Chain, OffPtr,
                                  MachinePointerInfo(SV, 8), MVT::i32);
  Chain = Offset.getValue(1);

  // Integer arguments are at base + offset; floating-point arguments in a
  // register slot (offset < 48) are 48 bytes below that.
  SDValue Addr = DAG.getNode(ISD::ADD, DL, MVT::i64, Base, Offset);
  SDValue InReg;
  if (VT.isFloatingPoint()) {
    InReg = DAG.getSetCC(DL, MVT::i64, Offset,
                         DAG.getConstant(48, DL, MVT::i64), ISD::SETULT);
    SDValue FPAddr = DAG.getNode(ISD::SUB, DL, MVT::i64, Addr,
                                 DAG.getConstant(48, DL, MVT::i64));
    Addr = DAG.getSelect(DL, MVT::i64, InReg, FPAddr, Addr);
  }

  // An argument occupies whole 8-byte slots, so a type wider than a register
  // -- X_floating, __int128 -- advances the offset by more than one.
  uint64_t Slots = alignTo(VT.getStoreSize(), 8);
  SDValue NextOff = DAG.getNode(ISD::ADD, DL, MVT::i64, Offset,
                                DAG.getConstant(Slots, DL, MVT::i64));
  Chain = DAG.getTruncStore(Chain, DL, NextOff, OffPtr,
                            MachinePointerInfo(SV, 8), MVT::i32);

  // A register slot holds what LowerFormalArguments saved there with an stt,
  // which is the T_floating form of the value whatever its type; a stack slot
  // holds what the caller stored, which for an f32 is the four-byte S_floating
  // form.  Reading an f32 slot the wrong way gives the high half of a double,
  // so the two are loaded separately and the same predicate picks between
  // them.  (The C ABI passes an unnamed float by reference, so this only
  // arises for hand-written va_arg; both addresses are inside the va area, so
  // loading both is safe.)
  if (VT == MVT::f32) {
    SDValue FPAddr =
        DAG.getNode(ISD::SUB, DL, MVT::i64,
                    DAG.getNode(ISD::ADD, DL, MVT::i64, Base, Offset),
                    DAG.getConstant(48, DL, MVT::i64));
    SDValue Wide =
        DAG.getLoad(MVT::f64, DL, Chain, FPAddr, MachinePointerInfo());
    SDValue Narrow = DAG.getLoad(
        MVT::f32, DL, Chain, DAG.getNode(ISD::ADD, DL, MVT::i64, Base, Offset),
        MachinePointerInfo());
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, Wide.getValue(1),
                        Narrow.getValue(1));
    SDValue Rounded =
        DAG.getNode(ISD::FP_ROUND, DL, MVT::f32, Wide,
                    DAG.getIntPtrConstant(0, DL, /*isTarget=*/true));
    SDValue Val = DAG.getSelect(DL, MVT::f32, InReg, Rounded, Narrow);
    return DAG.getMergeValues({Val, Chain}, DL);
  }

  return DAG.getLoad(VT, DL, Chain, Addr, MachinePointerInfo());
}

SDValue
AlphaTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_Alpha);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);
  for (unsigned I = 0, E = RVLocs.size(); I != E; ++I) {
    const CCValAssign &VA = RVLocs[I];
    assert(VA.isRegLoc() && "Alpha return values must be in registers");
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[I], Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  // A function returning in memory hands the buffer pointer it was given back
  // in $0, which is what GCC does.
  if (Register SRetReg =
          MF.getInfo<AlphaMachineFunctionInfo>()->getSRetReturnReg()) {
    SDValue Ptr = DAG.getCopyFromReg(Chain, DL, SRetReg, MVT::i64);
    Chain = DAG.getCopyToReg(Ptr.getValue(1), DL, Alpha::R0, Ptr, Glue);
    Glue = Chain.getValue(1);
    RetOps.push_back(DAG.getRegister(Alpha::R0, MVT::i64));
  }

  RetOps[0] = Chain;
  if (Glue.getNode())
    RetOps.push_back(Glue);

  return DAG.getNode(AlphaISD::RET_GLUE, DL, MVT::Other, RetOps);
}

// Rewrite an atomic pseudo into the ldq_l/stq_c loop pseudo that stands for its
// expansion, giving it a fresh virtual register for each scratch register that
// expansion needs.  The loop itself is not built here: the Alpha architecture
// requires that no memory access appear between the load locked and the store
// conditional, and anything built before register allocation can have a spill
// or a reload placed inside that window, which makes the store conditional fail
// every time round the loop.  AlphaExpandAtomicPseudo builds it after
// allocation instead.
static MachineBasicBlock *emitAtomicLoop(MachineInstr &MI,
                                         MachineBasicBlock *BB, unsigned LoopOpc,
                                         unsigned NumScratch, unsigned NumDefs,
                                         ArrayRef<int64_t> Modes) {
  MachineFunction &MF = *BB->getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  MachineInstrBuilder MIB = BuildMI(*BB, MI, MI.getDebugLoc(), TII.get(LoopOpc));
  // The pseudo's own results come first and keep the registers they were given.
  for (unsigned I = 0; I != NumDefs; ++I)
    MIB.addReg(MI.getOperand(I).getReg(), RegState::Define);
  for (unsigned I = 0; I != NumScratch; ++I)
    MIB.addReg(MRI.createVirtualRegister(&Alpha::GPRCRegClass),
               RegState::Define | RegState::Dead);
  for (unsigned I = NumDefs, E = MI.getNumOperands(); I != E; ++I)
    MIB.add(MI.getOperand(I));
  for (int64_t M : Modes)
    MIB.addImm(M);
  MIB.setMemRefs(MI.memoperands());

  MI.eraseFromParent();
  return BB;
}

MachineBasicBlock *
AlphaTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *MBB) const {
  switch (MI.getOpcode()) {
  case Alpha::ATOMIC_ADD_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::ADDQ, 0});
  case Alpha::ATOMIC_SUB_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::SUBQ, 0});
  case Alpha::ATOMIC_AND_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::AND, 0});
  case Alpha::ATOMIC_OR_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::BIS, 0});
  case Alpha::ATOMIC_XOR_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::XOR, 0});
  case Alpha::ATOMIC_XCHG_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {0, 0});
  case Alpha::ATOMIC_ADD_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::ADDQ, 1});
  case Alpha::ATOMIC_SUB_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::SUBQ, 1});
  case Alpha::ATOMIC_AND_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::AND, 1});
  case Alpha::ATOMIC_OR_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::BIS, 1});
  case Alpha::ATOMIC_XOR_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {Alpha::XOR, 1});
  case Alpha::ATOMIC_XCHG_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_RMW_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {0, 1});
  case Alpha::ATOMIC_CMPXCHG_I8:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_CAS_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {0});
  case Alpha::ATOMIC_CMPXCHG_I16:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_SUBWORD_CAS_LOOP,
                          /*NumScratch=*/4, /*NumDefs=*/1,
                          {1});
  case Alpha::SAFE_STOREI8:
    return emitAtomicLoop(MI, MBB, Alpha::SAFE_STORE_LOOP,
                          /*NumScratch=*/3, /*NumDefs=*/0,
                          {0});
  case Alpha::SAFE_STOREI16:
    return emitAtomicLoop(MI, MBB, Alpha::SAFE_STORE_LOOP,
                          /*NumScratch=*/3, /*NumDefs=*/0,
                          {1});
  case Alpha::SAFE_USTORE:
    return emitAtomicLoop(MI, MBB, Alpha::SAFE_USTORE_LOOP,
                          /*NumScratch=*/6, /*NumDefs=*/0,
                          {});
  case Alpha::ATOMIC_ADD_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::ADDQ, 0});
  case Alpha::ATOMIC_SUB_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::SUBQ, 0});
  case Alpha::ATOMIC_AND_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::AND, 0});
  case Alpha::ATOMIC_OR_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::BIS, 0});
  case Alpha::ATOMIC_XOR_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::XOR, 0});
  case Alpha::ATOMIC_XCHG_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {0, 0});
  case Alpha::ATOMIC_ADD_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::ADDQ, 1});
  case Alpha::ATOMIC_SUB_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::SUBQ, 1});
  case Alpha::ATOMIC_AND_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::AND, 1});
  case Alpha::ATOMIC_OR_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::BIS, 1});
  case Alpha::ATOMIC_XOR_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {Alpha::XOR, 1});
  case Alpha::ATOMIC_XCHG_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_RMW_LOOP,
                          /*NumScratch=*/1, /*NumDefs=*/1,
                          {0, 1});
  case Alpha::ATOMIC_CMPXCHG_I64:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_CAS_LOOP,
                          /*NumScratch=*/2, /*NumDefs=*/1,
                          {0});
  case Alpha::ATOMIC_CMPXCHG_I32:
    return emitAtomicLoop(MI, MBB, Alpha::ATOMIC_CAS_LOOP,
                          /*NumScratch=*/2, /*NumDefs=*/1,
                          {1});
  default:
    break;
  }

  // MOVi2f/MOVf2i reinterpret the 64 bits of a value.  The FIX extension moves
  // them directly with itoft/ftoit; otherwise they bounce through an 8-byte
  // stack slot, since Alpha has otherwise no integer/FP register move.
  MachineFunction &MF = *MBB->getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const DebugLoc &DL = MI.getDebugLoc();
  Register Dst = MI.getOperand(0).getReg();
  Register Src = MI.getOperand(1).getReg();

  if (Subtarget.hasFIX()) {
    unsigned Opc;
    switch (MI.getOpcode()) {
    default:
      llvm_unreachable("unexpected custom-inserted instruction");
    case Alpha::MOVi2f:
      Opc = Alpha::ITOFT;
      break;
    case Alpha::MOVf2i:
      Opc = Alpha::FTOIT;
      break;
    case Alpha::MOVi2f_S:
      Opc = Alpha::ITOFS;
      break;
    case Alpha::MOVf2i_S:
      Opc = Alpha::FTOIS;
      break;
    }
    BuildMI(*MBB, MI, DL, TII.get(Opc), Dst).addReg(Src);
    MI.eraseFromParent();
    return MBB;
  }

  // The store and its load are adjacent, so one slot serves every move in the
  // function rather than the frame growing by eight bytes per bitcast (and per
  // sitofp/fptosi, which are built on these).
  auto *FuncInfo = MF.getInfo<AlphaMachineFunctionInfo>();
  int FI = FuncInfo->getBitcastSlotIndex();
  if (FI == -1) {
    FI =
        MF.getFrameInfo().CreateStackObject(8, Align(8), /*isSpillSlot=*/false);
    FuncInfo->setBitcastSlotIndex(FI);
  }

  unsigned StoreOpc, LoadOpc;
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("unexpected custom-inserted instruction");
  case Alpha::MOVi2f:
    StoreOpc = Alpha::STQ;
    LoadOpc = Alpha::LDT;
    break;
  case Alpha::MOVf2i:
    StoreOpc = Alpha::STT;
    LoadOpc = Alpha::LDQ;
    break;
  case Alpha::MOVi2f_S:
    StoreOpc = Alpha::STL;
    LoadOpc = Alpha::LDS;
    break;
  case Alpha::MOVf2i_S:
    StoreOpc = Alpha::STS;
    LoadOpc = Alpha::LDL;
    break;
  }

  BuildMI(*MBB, MI, DL, TII.get(StoreOpc))
      .addReg(Src)
      .addFrameIndex(FI)
      .addImm(0);
  BuildMI(*MBB, MI, DL, TII.get(LoadOpc), Dst).addFrameIndex(FI).addImm(0);

  MI.eraseFromParent();
  return MBB;
}
