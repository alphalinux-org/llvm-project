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
  setOperationAction(ISD::BR_JT, MVT::Other, Expand);

  // Aligned integer loads and stores are atomic; barriers are inserted around
  // stronger orderings.  Wider atomic read-modify-writes are not handled yet.
  setMaxAtomicSizeInBitsSupported(64);

  // A fence needs looking at before it becomes an mb: see LowerOperation.
  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);

  // Global addresses are loaded from the GOT; constant pools are GP-relative.
  setOperationAction(ISD::GlobalAddress, MVT::i64, Custom);
  setOperationAction(ISD::ConstantPool, MVT::i64, Custom);

  // Unsigned integer/floating conversions expand through the signed ones.
  setOperationAction(ISD::UINT_TO_FP, MVT::i64, Expand);
  setOperationAction(ISD::FP_TO_UINT, MVT::i64, Expand);

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
  case AlphaISD::GPREL_HI:
    return "AlphaISD::GPREL_HI";
  case AlphaISD::GPREL_LO:
    return "AlphaISD::GPREL_LO";
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
  case ISD::ConstantPool:
    return LowerConstantPool(Op, DAG);
  default:
    llvm_unreachable("unexpected operation to lower");
  }
}

SDValue AlphaTargetLowering::LowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  // Loading a global's address establishes and uses the global pointer.
  DAG.getMachineFunction().getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  auto *N = cast<GlobalAddressSDNode>(Op);
  SDLoc DL(Op);
  SDValue TGA =
      DAG.getTargetGlobalAddress(N->getGlobal(), DL, MVT::i64, N->getOffset());
  return DAG.getNode(AlphaISD::LITERAL, DL, MVT::i64, TGA);
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
