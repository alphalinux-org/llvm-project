//===-- AlphaISelLowering.cpp - Alpha DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaISelLowering.h"
#include "Alpha.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
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

  // i64 select maps to a conditional move; expand SELECT_CC into SETCC+SELECT.
  setOperationAction(ISD::SELECT, MVT::i64, Legal);
  setOperationAction(ISD::SELECT_CC, MVT::i64, Expand);

  computeRegisterProperties(STI.getRegisterInfo());
}

const char *AlphaTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (static_cast<AlphaISD::NodeType>(Opcode)) {
  case AlphaISD::FIRST_NUMBER:
    break;
  case AlphaISD::RET_GLUE:
    return "AlphaISD::RET_GLUE";
  }
  return nullptr;
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

  for (const CCValAssign &VA : ArgLocs) {
    if (!VA.isRegLoc())
      report_fatal_error("Alpha stack arguments are not yet implemented");

    const TargetRegisterClass *RC = getRegClassForVT(VA.getLocVT());
    Register VReg = MF.addLiveIn(VA.getLocReg(), RC);
    SDValue Val = DAG.getCopyFromReg(Chain, DL, VReg, VA.getLocVT());
    InVals.push_back(Val);
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

  // The caller passes the return address in $26, which the return instruction
  // uses (directly for a leaf, or after the epilogue reloads it).  Mark it
  // live-in so that use has a definition reaching it from function entry.
  MF.getRegInfo().addLiveIn(Alpha::R26);

  return Chain;
}

bool AlphaTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);
  return CCInfo.CheckReturn(Outs, RetCC_Alpha);
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
