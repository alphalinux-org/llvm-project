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
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_CALLING_CONV_IMPL
#include "AlphaGenCallingConv.inc"

AlphaTargetLowering::AlphaTargetLowering(const AlphaTargetMachine &TM,
                                         const AlphaSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // Set up the register classes.
  addRegisterClass(MVT::i64, &Alpha::GPRCRegClass);
  addRegisterClass(MVT::f32, &Alpha::FPRCRegClass);
  addRegisterClass(MVT::f64, &Alpha::FPRCRegClass);

  setStackPointerRegisterToSaveRestore(Alpha::R30);

  // Comparison instructions leave 0 or 1 in the destination register.
  setBooleanContents(ZeroOrOneBooleanContent);

  // select maps to a conditional move; expand SELECT_CC into SETCC + SELECT.
  for (MVT VT : {MVT::i64, MVT::f32, MVT::f64}) {
    setOperationAction(ISD::SELECT, VT, Legal);
    setOperationAction(ISD::SELECT_CC, VT, Expand);
  }

  // Only BRCOND (branch on a 0/1 register) is supported; expand BR_CC into
  // SETCC + BRCOND, and there are no jump tables yet.
  setOperationAction(ISD::BR_CC, MVT::i64, Expand);
  setOperationAction(ISD::BR_CC, MVT::f32, Expand);
  setOperationAction(ISD::BR_CC, MVT::f64, Expand);
  // Jump tables are emitted as GP-relative offset tables and dispatched with a
  // load and an indirect jump.
  setOperationAction(ISD::JumpTable, MVT::i64, Custom);
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
  setOperationAction(ISD::ConstantPool, MVT::i64, Custom);

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

  // No byte-swap or rotate instructions; expand to shift/mask sequences.
  setOperationAction(ISD::BSWAP, MVT::i64, Expand);
  setOperationAction(ISD::ROTL, MVT::i64, Expand);
  setOperationAction(ISD::ROTR, MVT::i64, Expand);

  // umulh provides the high half of an unsigned 64x64 multiply; the signed high
  // multiply is expanded in terms of it.
  setOperationAction(ISD::MULHU, MVT::i64, Legal);
  setOperationAction(ISD::MULHS, MVT::i64, Expand);
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

  computeRegisterProperties(STI.getRegisterInfo());
}

TargetLowering::ConstraintType
AlphaTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1 && Constraint[0] == 'f')
    return C_RegisterClass;
  return TargetLowering::getConstraintType(Constraint);
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
      // Use a single-value-type register class so the operand's value type is
      // unambiguous (the shared FPRC would default to f32 and mishandle f64).
      // An integer bound to an FP register (e.g. loading the FPCR) is 64-bit,
      // so treat i64 like f64.
      if (VT == MVT::f64 || VT == MVT::i64)
        return std::make_pair(0U, &Alpha::F8RCRegClass);
      return std::make_pair(0U, &Alpha::F4RCRegClass);
    }
  }
  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
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
  case AlphaISD::DIVCALL:
    return "AlphaISD::DIVCALL";
  case AlphaISD::GPREL_HI:
    return "AlphaISD::GPREL_HI";
  case AlphaISD::GPREL_LO:
    return "AlphaISD::GPREL_LO";
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
  }
  return nullptr;
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
  case ISD::BR_JT:
    return LowerBR_JT(Op, DAG);
  case ISD::VASTART:
    return LowerVASTART(Op, DAG);
  case ISD::VAARG:
    return LowerVAARG(Op, DAG);
  case ISD::VACOPY:
    return LowerVACOPY(Op, DAG);
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::SREM:
  case ISD::UREM:
    return LowerDivRem(Op, DAG);
  default:
    llvm_unreachable("unexpected operation to lower");
  }
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
  // on every local string constant overflows that on a large link.
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
    Chain = DAG.getNode(AlphaISD::CALL, DL, {MVT::Other, MVT::Glue}, Ops);
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

SDValue AlphaTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  MachineFunction &MF = DAG.getMachineFunction();

  // No tail calls yet, and the caller uses the global pointer to load the
  // callee address and to reload gp afterwards.
  CLI.IsTailCall = false;
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CLI.CallConv, CLI.IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(CLI.Outs, CC_Alpha);
  unsigned NumBytes = CCInfo.getStackSize();

  SDValue Chain = DAG.getCALLSEQ_START(CLI.Chain, NumBytes, 0, DL);

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

  SDValue Callee = CLI.Callee;
  if (auto *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee =
        DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64,
                    DAG.getTargetGlobalAddress(G->getGlobal(), DL, MVT::i64));
  else if (auto *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64,
                         DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i64));
  // Otherwise the callee address is already a value; use it directly as the
  // procedure value for an indirect call.
  RegsToPass.emplace_back(Alpha::R27, Callee);

  SDValue Glue;
  for (auto &R : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, R.first, R.second, Glue);
    Glue = Chain.getValue(1);
  }

  SmallVector<SDValue, 8> Ops(1, Chain);
  for (auto &R : RegsToPass)
    Ops.push_back(DAG.getRegister(R.first, R.second.getValueType()));

  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CLI.CallConv);
  Ops.push_back(DAG.getRegisterMask(Mask));
  if (Glue.getNode())
    Ops.push_back(Glue);

  Chain = DAG.getNode(AlphaISD::CALL, DL, {MVT::Other, MVT::Glue}, Ops);
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
      Register FPReg = MF.addLiveIn(FPArgRegs[I], &Alpha::FPRCRegClass);
      SDValue FPVal = DAG.getCopyFromReg(Chain, DL, FPReg, MVT::f64);
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
  default:
    break;
  }

  // MOVi2f/MOVf2i reinterpret the 64 bits of a value by bouncing them through
  // an 8-byte stack slot, since Alpha has no direct integer/FP register move.
  MachineFunction &MF = *MBB->getParent();
  const TargetInstrInfo &TII = *MF.getSubtarget().getInstrInfo();
  const DebugLoc &DL = MI.getDebugLoc();
  int FI =
      MF.getFrameInfo().CreateStackObject(8, Align(8), /*isSpillSlot=*/false);
  Register Dst = MI.getOperand(0).getReg();
  Register Src = MI.getOperand(1).getReg();

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
  }

  BuildMI(*MBB, MI, DL, TII.get(StoreOpc))
      .addReg(Src)
      .addFrameIndex(FI)
      .addImm(0);
  BuildMI(*MBB, MI, DL, TII.get(LoadOpc), Dst).addFrameIndex(FI).addImm(0);

  MI.eraseFromParent();
  return MBB;
}
