//===-- AlphaCallLowering.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the lowering of LLVM calls to machine code calls for
// GlobalISel.  The assignments themselves come from the same tablegen calling
// convention the SelectionDAG path uses.
//
//===----------------------------------------------------------------------===//

#include "AlphaCallLowering.h"
#include "AlphaISelLowering.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineFrameInfo.h"

using namespace llvm;

// The calling convention is described in AlphaCallingConv.td.
#define GET_CALLING_CONV_IMPL
#include "AlphaGenCallingConv.inc"

namespace {

struct AlphaIncomingValueHandler : public CallLowering::IncomingValueHandler {
  AlphaIncomingValueHandler(MachineIRBuilder &MIRBuilder,
                            MachineRegisterInfo &MRI)
      : CallLowering::IncomingValueHandler(MIRBuilder, MRI) {}

  Register getStackAddress(uint64_t Size, int64_t Offset,
                           MachinePointerInfo &MPO,
                           ISD::ArgFlagsTy Flags) override {
    MachineFunction &MF = MIRBuilder.getMF();
    int FI = MF.getFrameInfo().CreateFixedObject(Size, Offset, true);
    MPO = MachinePointerInfo::getFixedStack(MF, FI);
    return MIRBuilder.buildFrameIndex(LLT::pointer(0, 64), FI).getReg(0);
  }

  void assignValueToAddress(Register ValVReg, Register Addr, LLT MemTy,
                            const MachinePointerInfo &MPO,
                            const CCValAssign &VA) override {
    MachineFunction &MF = MIRBuilder.getMF();
    auto *MMO = MF.getMachineMemOperand(MPO, MachineMemOperand::MOLoad, MemTy,
                                        inferAlignFromPtrInfo(MF, MPO));
    MIRBuilder.buildLoad(ValVReg, Addr, *MMO);
  }

  void assignValueToReg(Register ValVReg, Register PhysReg,
                        const CCValAssign &VA,
                        ISD::ArgFlagsTy Flags = {}) override {
    markPhysRegUsed(PhysReg);
    IncomingValueHandler::assignValueToReg(ValVReg, PhysReg, VA);
  }

  virtual void markPhysRegUsed(MCRegister PhysReg) {
    MIRBuilder.getMRI()->addLiveIn(PhysReg);
    MIRBuilder.getMBB().addLiveIn(PhysReg);
  }
};

struct AlphaFormalArgHandler : public AlphaIncomingValueHandler {
  AlphaFormalArgHandler(MachineIRBuilder &MIRBuilder, MachineRegisterInfo &MRI)
      : AlphaIncomingValueHandler(MIRBuilder, MRI) {}
};

struct AlphaCallReturnHandler : public AlphaIncomingValueHandler {
  AlphaCallReturnHandler(MachineIRBuilder &MIRBuilder, MachineRegisterInfo &MRI,
                         MachineInstrBuilder &MIB)
      : AlphaIncomingValueHandler(MIRBuilder, MRI), MIB(MIB) {}

  void markPhysRegUsed(MCRegister PhysReg) override {
    MIB.addDef(PhysReg, RegState::Implicit);
  }

  MachineInstrBuilder &MIB;
};

struct AlphaOutgoingValueHandler : public CallLowering::OutgoingValueHandler {
  AlphaOutgoingValueHandler(MachineIRBuilder &MIRBuilder,
                            MachineRegisterInfo &MRI, MachineInstrBuilder MIB)
      : CallLowering::OutgoingValueHandler(MIRBuilder, MRI), MIB(MIB) {}

  Register getStackAddress(uint64_t Size, int64_t Offset,
                           MachinePointerInfo &MPO,
                           ISD::ArgFlagsTy Flags) override {
    MachineFunction &MF = MIRBuilder.getMF();
    LLT p0 = LLT::pointer(0, 64);
    LLT s64 = LLT::scalar(64);
    auto SPReg = MIRBuilder.buildCopy(p0, Register(Alpha::R30)).getReg(0);
    auto OffsetReg = MIRBuilder.buildConstant(s64, Offset);
    auto AddrReg = MIRBuilder.buildPtrAdd(p0, SPReg, OffsetReg);
    MPO = MachinePointerInfo::getStack(MF, Offset);
    return AddrReg.getReg(0);
  }

  void assignValueToAddress(Register ValVReg, Register Addr, LLT MemTy,
                            const MachinePointerInfo &MPO,
                            const CCValAssign &VA) override {
    MachineFunction &MF = MIRBuilder.getMF();
    Register ExtReg = extendRegister(ValVReg, VA);
    auto *MMO = MF.getMachineMemOperand(MPO, MachineMemOperand::MOStore, MemTy,
                                        inferAlignFromPtrInfo(MF, MPO));
    MIRBuilder.buildStore(ExtReg, Addr, *MMO);
  }

  void assignValueToReg(Register ValVReg, Register PhysReg,
                        const CCValAssign &VA,
                        ISD::ArgFlagsTy Flags = {}) override {
    Register ExtReg = extendRegister(ValVReg, VA);
    MIRBuilder.buildCopy(PhysReg, ExtReg);
    MIB.addUse(PhysReg, RegState::Implicit);
  }

  MachineInstrBuilder MIB;
};

} // end anonymous namespace

AlphaCallLowering::AlphaCallLowering(const AlphaTargetLowering &TLI)
    : CallLowering(&TLI) {}

bool AlphaCallLowering::lowerReturn(MachineIRBuilder &MIRBuilder,
                                    const Value *Val, ArrayRef<Register> VRegs,
                                    FunctionLoweringInfo &FLI,
                                    Register SwiftErrorVReg) const {
  assert(!Val == VRegs.empty() && "Return value without a vreg");

  MachineFunction &MF = MIRBuilder.getMF();
  const Function &F = MF.getFunction();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  auto MIB = MIRBuilder.buildInstrNoInsert(Alpha::RET);

  if (!VRegs.empty()) {
    const DataLayout &DL = F.getDataLayout();
    SmallVector<ArgInfo, 4> SplitArgs;
    ArgInfo OrigArg{VRegs, Val->getType(), 0};
    setArgFlags(OrigArg, AttributeList::ReturnIndex, DL, F);
    splitToValueTypes(OrigArg, SplitArgs, DL, F.getCallingConv());

    OutgoingValueAssigner Assigner(RetCC_Alpha);
    AlphaOutgoingValueHandler Handler(MIRBuilder, MRI, MIB);
    if (!determineAndHandleAssignments(Handler, Assigner, SplitArgs, MIRBuilder,
                                       F.getCallingConv(), F.isVarArg()))
      return false;
  }

  MIRBuilder.insertInstr(MIB);
  return true;
}

bool AlphaCallLowering::lowerFormalArguments(MachineIRBuilder &MIRBuilder,
                                             const Function &F,
                                             ArrayRef<ArrayRef<Register>> VRegs,
                                             FunctionLoweringInfo &FLI) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const DataLayout &DL = F.getDataLayout();

  // A variadic function needs the register save area the SelectionDAG path
  // builds, which this does not do yet.
  if (F.isVarArg())
    return false;

  SmallVector<ArgInfo, 8> SplitArgs;
  unsigned Idx = 0;
  for (const auto &Arg : F.args()) {
    ArgInfo OrigArg{VRegs[Idx], Arg.getType(), Idx};
    setArgFlags(OrigArg, Idx + AttributeList::FirstArgIndex, DL, F);
    splitToValueTypes(OrigArg, SplitArgs, DL, F.getCallingConv());
    ++Idx;
  }

  IncomingValueAssigner Assigner(CC_Alpha);
  AlphaFormalArgHandler Handler(MIRBuilder, MRI);
  if (!determineAndHandleAssignments(Handler, Assigner, SplitArgs, MIRBuilder,
                                     F.getCallingConv(), F.isVarArg()))
    return false;

  // A function that makes a call needs the return address, which arrives in
  // $26 and is preserved by the frame lowering.
  MRI.addLiveIn(Alpha::R26);
  MIRBuilder.getMBB().addLiveIn(Alpha::R26);
  return true;
}

bool AlphaCallLowering::lowerCall(MachineIRBuilder &MIRBuilder,
                                  CallLoweringInfo &Info) const {
  MachineFunction &MF = MIRBuilder.getMF();
  const Function &F = MF.getFunction();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const AlphaSubtarget &STI = MF.getSubtarget<AlphaSubtarget>();
  const DataLayout &DL = F.getDataLayout();

  // Only a direct call to a named function is handled here.  An indirect call
  // needs the procedure value in $27, which the direct call instruction sets up
  // itself, and a variadic one needs its own argument rules.
  if (!Info.Callee.isGlobal() && !Info.Callee.isSymbol())
    return false;
  if (Info.IsMustTailCall)
    return false;

  // A variadic call needs nothing special here: the callee saves both the
  // integer and the floating-point argument registers, so an argument only has
  // to reach the one its type selects.

  // The call reads the global pointer to reach the callee's address, so the
  // prologue has to establish one.
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  SmallVector<ArgInfo, 8> OutArgs;
  for (auto &OrigArg : Info.OrigArgs)
    splitToValueTypes(OrigArg, OutArgs, DL, Info.CallConv);

  SmallVector<ArgInfo, 4> InArgs;
  if (!Info.OrigRet.Ty->isVoidTy())
    splitToValueTypes(Info.OrigRet, InArgs, DL, Info.CallConv);

  auto CallSeqStart = MIRBuilder.buildInstr(Alpha::ADJCALLSTACKDOWN);

  auto MIB = MIRBuilder.buildInstrNoInsert(Alpha::JSRd);
  MIB.add(Info.Callee);
  MIB.addRegMask(
      STI.getRegisterInfo()->getCallPreservedMask(MF, Info.CallConv));

  OutgoingValueAssigner ArgAssigner(CC_Alpha);
  AlphaOutgoingValueHandler ArgHandler(MIRBuilder, MRI, MIB);
  if (!determineAndHandleAssignments(ArgHandler, ArgAssigner, OutArgs,
                                     MIRBuilder, Info.CallConv, Info.IsVarArg))
    return false;

  MIRBuilder.insertInstr(MIB);

  if (!InArgs.empty()) {
    IncomingValueAssigner RetAssigner(RetCC_Alpha);
    AlphaCallReturnHandler RetHandler(MIRBuilder, MRI, MIB);
    if (!determineAndHandleAssignments(RetHandler, RetAssigner, InArgs,
                                       MIRBuilder, Info.CallConv,
                                       Info.IsVarArg))
      return false;
  }

  CallSeqStart.addImm(ArgAssigner.StackSize).addImm(0);
  MIRBuilder.buildInstr(Alpha::ADJCALLSTACKUP)
      .addImm(ArgAssigner.StackSize)
      .addImm(0);
  return true;
}
