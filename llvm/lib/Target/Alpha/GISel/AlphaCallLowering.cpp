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
#include "Alpha.h"
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
    // The outgoing argument area is a run of 8-byte slots at multiples of eight
    // from the stack pointer, so a slot is always 8-byte aligned.  Do not ask
    // inferAlignFromPtrInfo here: the MPO is a plain Stack pseudo-source-value,
    // which has no fixed frame index, so it falls through to Align(1) and the
    // selector would treat every stack-passed argument as a misaligned store.
    auto *MMO = MF.getMachineMemOperand(MPO, MachineMemOperand::MOStore, MemTy,
                                        std::max(inferAlignFromPtrInfo(MF, MPO),
                                                 Align(8)));
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

// A floating value in a signature needs a floating argument or return register,
// which -mno-fp-regs takes away.  The SelectionDAG path does not make the
// floating types legal under that option at all: it lowers such a signature to
// the soft-float convention, or diagnoses the one case that has no soft-float
// form (AlphaISelLowering.cpp).  Neither is done here, so hand the function to
// that path whole rather than assign it a register allocation cannot provide --
// which is what invariant B3 catches, after the fact.  AlphaLegalizerInfo does
// the same for a floating operation in the body.
static bool needsFPRegs(const AlphaSubtarget &STI,
                        ArrayRef<CallLowering::ArgInfo> Args) {
  return STI.hasNoFPRegs() && any_of(Args, [](const CallLowering::ArgInfo &A) {
           return A.Ty->isFPOrFPVectorTy();
         });
}

// A return that RetCC_Alpha cannot assign has to be demoted to sret by the
// caller, the same decision AlphaTargetLowering::CanLowerReturn makes for the
// SelectionDAG path.  The default answers yes to everything, which would leave
// lowerReturn to fail on the unassignable value instead.
bool AlphaCallLowering::canLowerReturn(MachineFunction &MF,
                                       CallingConv::ID CallConv,
                                       SmallVectorImpl<BaseArgInfo> &Outs,
                                       bool IsVarArg) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, MF.getFunction().getContext());
  return checkReturn(CCInfo, Outs, RetCC_Alpha);
}

bool AlphaCallLowering::lowerReturn(MachineIRBuilder &MIRBuilder,
                                    const Value *Val, ArrayRef<Register> VRegs,
                                    FunctionLoweringInfo &FLI,
                                    Register SwiftErrorVReg) const {
  assert(!Val == VRegs.empty() && "Return value without a vreg");

  MachineFunction &MF = MIRBuilder.getMF();
  const Function &F = MF.getFunction();
  MachineRegisterInfo &MRI = MF.getRegInfo();

  if (Val && needsFPRegs(MF.getSubtarget<AlphaSubtarget>(),
                         {CallLowering::ArgInfo{VRegs, Val->getType(), 0}}))
    return false;

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

  // A function returning in memory hands the buffer pointer it was given back
  // in $0, which is what the SelectionDAG path and GCC both do.  The calling
  // convention tables say nothing about this, so it has to be done by hand on
  // both paths.
  if (Register SRetReg =
          MF.getInfo<AlphaMachineFunctionInfo>()->getSRetReturnReg()) {
    MIRBuilder.buildCopy(Register(Alpha::R0), SRetReg);
    MIB.addUse(Alpha::R0, RegState::Implicit);
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

  SmallVector<ArgInfo, 8> SplitArgs;
  unsigned Idx = 0;
  for (const auto &Arg : F.args()) {
    ArgInfo OrigArg{VRegs[Idx], Arg.getType(), Idx};
    setArgFlags(OrigArg, Idx + AttributeList::FirstArgIndex, DL, F);
    splitToValueTypes(OrigArg, SplitArgs, DL, F.getCallingConv());
    ++Idx;
  }

  if (needsFPRegs(MF.getSubtarget<AlphaSubtarget>(), SplitArgs))
    return false;

  // The assignments are made here rather than through
  // determineAndHandleAssignments so that the locations stay in scope: a
  // variadic function needs to know how many of the six argument registers the
  // named arguments used before it can save the rest.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(F.getCallingConv(), F.isVarArg(), MF, ArgLocs, F.getContext());
  IncomingValueAssigner Assigner(CC_Alpha);
  AlphaIncomingValueHandler Handler(MIRBuilder, MRI);
  if (!determineAssignments(Assigner, SplitArgs, CCInfo))
    return false;
  if (!handleAssignments(Handler, SplitArgs, CCInfo, ArgLocs, MIRBuilder))
    return false;

  if (F.isVarArg() && !saveVarArgRegisters(MIRBuilder, CCInfo, ArgLocs))
    return false;

  // Keep the hidden result pointer of a function returning in memory: it is
  // returned again in $0 (see lowerReturn).
  if (!F.arg_empty() && F.getArg(0)->hasStructRetAttr()) {
    assert(
        llvm::none_of(llvm::drop_begin(F.args()),
                      [](const Argument &A) { return A.hasStructRetAttr(); }) &&
        "sret is the first argument");
    assert(VRegs[0].size() == 1 && "sret pointer is one value");
    auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
    Register Reg = MRI.createGenericVirtualRegister(LLT::pointer(0, 64));
    MRI.setRegClass(Reg, &Alpha::GPRCRegClass);
    FI->setSRetReturnReg(Reg);
    MIRBuilder.buildCopy(Reg, VRegs[0][0]);
  }

  // A function that makes a call needs the return address, which arrives in
  // $26 and is preserved by the frame lowering.
  MRI.addLiveIn(Alpha::R26);
  MIRBuilder.getMBB().addLiveIn(Alpha::R26);
  return true;
}

// Fill the argument save area a later va_arg reads, exactly as the
// IsVarArg arm of LowerFormalArguments does.  Slot N of the argument list
// lives at IntBase + N*8 for every N: the six register slots are the save area
// itself, and slot 6 onwards are the caller's stack arguments, which start at
// the incoming stack pointer.  So the integer save area sits immediately below
// the incoming stack pointer at a fixed -48, with the floating-point area
// below it at -96, and neither offset depends on how much stack the named
// arguments used.
bool AlphaCallLowering::saveVarArgRegisters(
    MachineIRBuilder &MIRBuilder, const CCState &CCInfo,
    ArrayRef<CCValAssign> ArgLocs) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const AlphaSubtarget &STI = MF.getSubtarget<AlphaSubtarget>();
  const LLT s64 = LLT::scalar(64);
  const LLT p0 = LLT::pointer(0, 64);

  static const MCPhysReg IntArgRegs[] = {Alpha::R16, Alpha::R17, Alpha::R18,
                                         Alpha::R19, Alpha::R20, Alpha::R21};
  static const MCPhysReg FPArgRegs[] = {Alpha::F16, Alpha::F17, Alpha::F18,
                                        Alpha::F19, Alpha::F20, Alpha::F21};

  unsigned NumNamed = CCInfo.getStackSize() / 8;
  for (const CCValAssign &VA : ArgLocs)
    if (VA.isRegLoc())
      ++NumNamed;

  int IntFI = MFI.CreateFixedObject(48, -48, /*IsImmutable=*/false);
  int FpFI = MFI.CreateFixedObject(48, -96, /*IsImmutable=*/false);
  auto IntBase = MIRBuilder.buildFrameIndex(p0, IntFI);
  auto FpBase = MIRBuilder.buildFrameIndex(p0, FpFI);

  for (unsigned I = NumNamed; I < 6; ++I) {
    MRI.addLiveIn(IntArgRegs[I]);
    MIRBuilder.getMBB().addLiveIn(IntArgRegs[I]);
    auto IntVal = MIRBuilder.buildCopy(s64, Register(IntArgRegs[I]));
    auto Offset = MIRBuilder.buildConstant(s64, I * 8);
    auto IntPtr = MIRBuilder.buildPtrAdd(p0, IntBase, Offset);
    MIRBuilder.buildStore(
        IntVal, IntPtr,
        *MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, IntFI, I * 8),
            MachineMemOperand::MOStore, 8, Align(8)));

    // With -mno-fp-regs there are no floating-point argument registers to read:
    // the file is out of the register classes altogether.  A floating-point
    // argument arrives in an integer register under that flag, so fill the
    // floating-point save area from the integer registers too, which is what
    // gcc's alpha_setup_incoming_varargs does -- the register it reads is
    // `16 + cum + TARGET_FPREGS*32', the integer one when TARGET_FPREGS is
    // zero.
    Register FPVal = IntVal.getReg(0);
    if (!STI.hasNoFPRegs()) {
      MRI.addLiveIn(FPArgRegs[I]);
      MIRBuilder.getMBB().addLiveIn(FPArgRegs[I]);
      FPVal = MIRBuilder.buildCopy(LLT::float64(), Register(FPArgRegs[I]))
                  .getReg(0);
    }
    auto FPPtr = MIRBuilder.buildPtrAdd(p0, FpBase, Offset);
    MIRBuilder.buildStore(
        FPVal, FPPtr,
        *MF.getMachineMemOperand(
            MachinePointerInfo::getFixedStack(MF, FpFI, I * 8),
            MachineMemOperand::MOStore, 8, Align(8)));
  }

  auto *FI = MF.getInfo<AlphaMachineFunctionInfo>();
  FI->setVarArgsFrameIndex(IntFI);
  FI->setVarArgsOffset(NumNamed * 8);
  return true;
}

// The same conditions AlphaTargetLowering::isEligibleForTailCallOptimization
// applies on the SelectionDAG path; see the comments there for why each one is
// needed.  The caller has already established that the call is in tail
// position and that the return values line up.
bool AlphaCallLowering::isEligibleForTailCall(MachineFunction &MF,
                                              const CallLoweringInfo &Info,
                                              unsigned NumStackBytes,
                                              ArrayRef<ArgInfo> OutArgs) const {
  CallingConv::ID CallerCC = MF.getFunction().getCallingConv();
  if (CallerCC != Info.CallConv)
    return false;
  if (Info.CallConv != CallingConv::C && Info.CallConv != CallingConv::Fast)
    return false;
  if (Info.IsVarArg || NumStackBytes != 0)
    return false;
  for (const ArgInfo &Arg : OutArgs)
    if (Arg.Flags[0].isByVal())
      return false;
  // Only a callee known to run on this function's gp: the jump leaves whatever
  // gp the callee loaded in $29, and our caller is entitled to find its own
  // there.  An indirect callee cannot be asked, and neither can a bare symbol.
  if (!Info.Callee.isGlobal())
    return false;
  return getTLI<AlphaTargetLowering>()->calleeSharesGP(
      *Info.Callee.getGlobal());
}

bool AlphaCallLowering::lowerTailCall(MachineIRBuilder &MIRBuilder,
                                      CallLoweringInfo &Info,
                                      SmallVectorImpl<ArgInfo> &OutArgs) const {
  MachineFunction &MF = MIRBuilder.getMF();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const AlphaSubtarget &STI = MF.getSubtarget<AlphaSubtarget>();
  const GlobalValue &GV = *Info.Callee.getGlobal();

  // A tail callee always shares our gp, so the two forms that load a procedure
  // value through the GOT with a hint relocation cannot arise here: the choice
  // is between the local literal load and, under -msmall-text, a plain branch.
  unsigned Opc = STI.hasSmallText() && isAlphaDirectlyNameable(GV)
                     ? Alpha::TCRETURNbr
                     : Alpha::TCRETURNdl;

  auto MIB = MIRBuilder.buildInstrNoInsert(Opc);
  MIB.add(Info.Callee);

  // No register mask: nothing is live past the jump.  No call sequence either
  // -- the callee takes nothing on the stack, so it reuses the frame we are
  // about to tear down, and the epilogue is inserted before this terminator.
  OutgoingValueAssigner ArgAssigner(CC_Alpha);
  AlphaOutgoingValueHandler ArgHandler(MIRBuilder, MRI, MIB);
  if (!determineAndHandleAssignments(ArgHandler, ArgAssigner, OutArgs,
                                     MIRBuilder, Info.CallConv, Info.IsVarArg))
    return false;

  MIRBuilder.insertInstr(MIB);

  MF.getFrameInfo().setHasTailCall();
  Info.LoweredTailCall = true;
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
  if (needsFPRegs(STI, Info.OrigArgs) ||
      (Info.OrigRet.Ty && needsFPRegs(STI, {Info.OrigRet})))
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

  // Whether the call can become a jump depends on how much stack it needs, so
  // the assignment has to be run once before anything is built.  The handler
  // below runs it again for real; this one only counts.
  unsigned NumStackBytes = 0;
  {
    SmallVector<CCValAssign, 16> ArgLocs;
    CCState CCInfo(Info.CallConv, Info.IsVarArg, MF, ArgLocs, F.getContext());
    OutgoingValueAssigner CountAssigner(CC_Alpha);
    if (!determineAssignments(CountAssigner, OutArgs, CCInfo))
      return false;
    NumStackBytes = CCInfo.getStackSize();
  }

  bool IsTailCall = Info.IsTailCall &&
                    isEligibleForTailCall(MF, Info, NumStackBytes, OutArgs);
  // musttail is a guarantee the front end has already made to the caller, so a
  // call that cannot be turned into a jump is not something to paper over.
  // Falling back to the SelectionDAG path lets it make the same decision and,
  // where it also refuses, report it.
  if (Info.IsMustTailCall && !IsTailCall)
    return false;

  if (IsTailCall)
    return lowerTailCall(MIRBuilder, Info, OutArgs);

  SmallVector<ArgInfo, 4> InArgs;
  if (!Info.OrigRet.Ty->isVoidTy())
    splitToValueTypes(Info.OrigRet, InArgs, DL, Info.CallConv);

  auto CallSeqStart = MIRBuilder.buildInstr(Alpha::ADJCALLSTACKDOWN);

  // Pick the same call instruction the SelectionDAG path would: a hint
  // relocation only where the linker may not relax the call away, and under
  // -msmall-text a single bsr to a callee the linker resolves itself.
  unsigned CallOpc = Alpha::JSRd;
  if (Info.Callee.isGlobal()) {
    const GlobalValue &GV = *Info.Callee.getGlobal();
    if (STI.hasSmallText() && isAlphaDirectlyNameable(GV))
      CallOpc = Alpha::CALLbsr;
    else if (GV.isDSOLocal())
      CallOpc = Alpha::JSRdl;
  }

  auto MIB = MIRBuilder.buildInstrNoInsert(CallOpc);
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
