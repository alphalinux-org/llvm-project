//===-- AlphaISelLowering.cpp - Alpha DAG Lowering Implementation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaISelLowering.h"
#include "Alpha.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

AlphaTargetLowering::AlphaTargetLowering(const AlphaTargetMachine &TM,
                                         const AlphaSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {
  // Set up the register classes.
  addRegisterClass(MVT::i64, &Alpha::GPRCRegClass);
  addRegisterClass(MVT::f32, &Alpha::FPRCRegClass);
  addRegisterClass(MVT::f64, &Alpha::FPRCRegClass);

  setStackPointerRegisterToSaveRestore(Alpha::R30);

  computeRegisterProperties(STI.getRegisterInfo());
}

SDValue AlphaTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  if (!Ins.empty())
    report_fatal_error("Alpha argument lowering is not yet implemented");
  return Chain;
}

SDValue
AlphaTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  report_fatal_error("Alpha return lowering is not yet implemented");
}
