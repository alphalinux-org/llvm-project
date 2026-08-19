//===- AlphaISelDAGToDAG.cpp - A dag to dag inst selector for Alpha -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Alpha target.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"

using namespace llvm;

#define DEBUG_TYPE "alpha-isel"

namespace {

class AlphaDAGToDAGISel : public SelectionDAGISel {
  const AlphaSubtarget *Subtarget = nullptr;

public:
  AlphaDAGToDAGISel(AlphaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISel(TM, OptLevel) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<AlphaSubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  void Select(SDNode *Node) override;

  // Match an address of the form base + 16-bit signed displacement (or a
  // frame index) into (Base, Offset).
  bool SelectADDRri(SDValue Addr, SDValue &Base, SDValue &Offset);

  // Emit a constant-materialization sequence on top of Base and return the
  // value the last step leaves behind.
  SDValue emitConstantSteps(ArrayRef<Alpha::ConstantStep> Steps, SDValue Base,
                            const SDLoc &DL);

  // Match any address as a single register operand.
  bool SelectAddrReg(SDValue Addr, SDValue &Reg) {
    Reg = Addr;
    return true;
  }

  // Addressing-mode selection for inline-asm memory ("m"/"o") operands.
  bool SelectInlineAsmMemoryOperand(const SDValue &Op,
                                    InlineAsm::ConstraintCode ConstraintID,
                                    std::vector<SDValue> &OutOps) override {
    switch (ConstraintID) {
    default:
      return true;
    case InlineAsm::ConstraintCode::o:
    case InlineAsm::ConstraintCode::m: {
      SDValue Base, Offset;
      SelectADDRri(Op, Base, Offset);
      OutOps.push_back(Base);
      OutOps.push_back(Offset);
      return false;
    }
    }
  }

#include "AlphaGenDAGISel.inc"
};

class AlphaDAGToDAGISelLegacy : public SelectionDAGISelLegacy {
public:
  static char ID;

  AlphaDAGToDAGISelLegacy(AlphaTargetMachine &TM, CodeGenOptLevel OptLevel)
      : SelectionDAGISelLegacy(
            ID, std::make_unique<AlphaDAGToDAGISel>(TM, OptLevel)) {}

  StringRef getPassName() const override {
    return "Alpha DAG->DAG Pattern Instruction Selection";
  }
};

} // end anonymous namespace

char AlphaDAGToDAGISelLegacy::ID = 0;

void AlphaDAGToDAGISel::Select(SDNode *Node) {
  if (Node->isMachineOpcode()) {
    Node->setNodeId(-1);
    return;
  }

  // Without BWX a byte or word store is a read-modify-write of the quadword
  // holding the field.  Select it to a single instruction that is expanded
  // after register allocation: stores of different bytes of one quadword do not
  // alias, so they are unordered in the DAG, and a read-modify-write built here
  // out of separate instructions could have another byte's write scheduled
  // between its load and its store, which would then be lost.
  if (auto *ST = dyn_cast<StoreSDNode>(Node)) {
    EVT MemVT = ST->getMemoryVT();
    bool IsByte = MemVT == MVT::i8;
    if (ST->isTruncatingStore() && (IsByte || MemVT == MVT::i16) &&
        ST->getAlign() >= MemVT.getStoreSize() && !Subtarget->hasBWX() &&
        !Subtarget->hasSafeBWA()) {
      SDLoc DL(Node);
      MachineSDNode *Store = CurDAG->getMachineNode(
          IsByte ? Alpha::RMW_STOREI8 : Alpha::RMW_STOREI16, DL,
          {MVT::i64, MVT::i64, MVT::Other},
          {ST->getValue(), ST->getBasePtr(), ST->getChain()});
      MachineFunction &MF = CurDAG->getMachineFunction();
      auto Flags = ST->isVolatile() ? MachineMemOperand::MOVolatile
                                    : MachineMemOperand::MONone;
      CurDAG->setNodeMemRefs(
          Store, {MF.getMachineMemOperand(MachinePointerInfo(),
                                          Flags | MachineMemOperand::MOLoad |
                                              MachineMemOperand::MOStore,
                                          8, Align(8))});
      ReplaceUses(SDValue(Node, 0), SDValue(Store, 2));
      CurDAG->RemoveDeadNode(Node);
      return;
    }
  }

  // A misaligned store carries four scratch registers, which a pattern cannot
  // describe, so build the instruction here.
  if (Node->getOpcode() == AlphaISD::USTORE) {
    SDLoc DL(Node);
    auto *Mem = cast<MemSDNode>(Node);
    SDValue Ops[] = {
        Node->getOperand(1), Node->getOperand(2),
        CurDAG->getTargetConstant(
            cast<ConstantSDNode>(Node->getOperand(3))->getZExtValue(), DL,
            MVT::i64),
        Node->getOperand(0)};
    MachineSDNode *Store = CurDAG->getMachineNode(
        Alpha::RMW_USTORE, DL,
        {MVT::i64, MVT::i64, MVT::i64, MVT::i64, MVT::Other}, Ops);
    // The expansion reads the one or two quadwords the field falls in before
    // writing them back, so the access is a load as well as a store and covers
    // the whole of both quadwords -- and it keeps whatever the original store
    // said about being volatile.  Described against no particular object, as
    // the byte store above is, since which quadwords are touched is not known
    // until run time.
    MachineFunction &MF = CurDAG->getMachineFunction();
    auto Flags = Mem->isVolatile() ? MachineMemOperand::MOVolatile
                                   : MachineMemOperand::MONone;
    CurDAG->setNodeMemRefs(
        Store, {MF.getMachineMemOperand(MachinePointerInfo(),
                                        Flags | MachineMemOperand::MOLoad |
                                            MachineMemOperand::MOStore,
                                        16, Align(8))});
    ReplaceUses(SDValue(Node, 0), SDValue(Store, 4));
    CurDAG->RemoveDeadNode(Node);
    return;
  }

  // Materialize a frame-index address with lda; the displacement (0) follows
  // the base so eliminateFrameIndex can rewrite it.
  if (Node->getOpcode() == ISD::FrameIndex) {
    SDLoc DL(Node);
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, MVT::i64);
    SDValue Zero = CurDAG->getTargetConstant(0, DL, MVT::i64);
    ReplaceNode(Node,
                CurDAG->getMachineNode(Alpha::LEA, DL, MVT::i64, TFI, Zero));
    return;
  }

  // Materialize a constant that does not fit in the 16-bit `lda` displacement.
  // 16-bit constants are handled by a pattern; a 32-bit constant is an ldah/lda
  // pair (with the 0x8000 high half split into two ldah); wider constants go
  // inline or, failing that, into the constant pool.
  if (Node->getOpcode() == ISD::Constant && Node->getValueType(0) == MVT::i64) {
    int64_t V = cast<ConstantSDNode>(Node)->getSExtValue();
    SDLoc DL(Node);
    SmallVector<Alpha::ConstantStep, 8> Steps;
    if (!isInt<16>(V) && isInt<32>(V)) {
      Alpha::buildConstant32Steps(static_cast<int32_t>(V), Steps);
      SDValue Zero = CurDAG->getRegister(Alpha::R31, MVT::i64);
      ReplaceNode(Node, emitConstantSteps(Steps, Zero, DL).getNode());
      return;
    }
    if (!isInt<32>(V)) {
      // Build a wide constant inline (ldah/lda of each 32-bit half combined
      // with a shift) when the constant pool must be avoided; otherwise place
      // it in the pool and load it GP-relative.
      if (Subtarget->hasBuildConstants()) {
        Alpha::buildConstantSteps(V, Steps);
        SDValue Zero = CurDAG->getRegister(Alpha::R31, MVT::i64);
        ReplaceNode(Node, emitConstantSteps(Steps, Zero, DL).getNode());
        return;
      }
      CurDAG->getMachineFunction()
          .getInfo<AlphaMachineFunctionInfo>()
          ->setUsesGP();
      const Constant *CV =
          ConstantInt::get(Type::getInt64Ty(*CurDAG->getContext()), V);
      SDValue CPI = CurDAG->getTargetConstantPool(CV, MVT::i64, Align(8));
      SDValue GP = CurDAG->getRegister(Alpha::R29, MVT::i64);
      SDNode *High =
          CurDAG->getMachineNode(Alpha::LDAHg, DL, MVT::i64, CPI, GP);
      // ldq with a !gprellow displacement folds the low part into the load.
      SDNode *Load = CurDAG->getMachineNode(Alpha::LDQg, DL, MVT::i64, CPI,
                                            SDValue(High, 0));
      ReplaceNode(Node, Load);
      return;
    }
  }

  SelectCode(Node);
}

SDValue
AlphaDAGToDAGISel::emitConstantSteps(ArrayRef<Alpha::ConstantStep> Steps,
                                     SDValue Base, const SDLoc &DL) {
  SDValue Cur = Base;
  for (const Alpha::ConstantStep &S : Steps) {
    SDValue Imm = CurDAG->getTargetConstant(S.Imm, DL, MVT::i64);
    // sll takes its register operand first, ldah and lda their displacement.
    Cur = SDValue(S.Opc == Alpha::SLLi
                      ? CurDAG->getMachineNode(S.Opc, DL, MVT::i64, Cur, Imm)
                      : CurDAG->getMachineNode(S.Opc, DL, MVT::i64, Imm, Cur),
                  0);
  }
  return Cur;
}

bool AlphaDAGToDAGISel::SelectADDRri(SDValue Addr, SDValue &Base,
                                     SDValue &Offset) {
  SDLoc DL(Addr);

  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i64);
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i64);
    return true;
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    auto *CN = cast<ConstantSDNode>(Addr.getOperand(1));
    if (isInt<16>(CN->getSExtValue())) {
      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr.getOperand(0)))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i64);
      else
        Base = Addr.getOperand(0);
      Offset = CurDAG->getTargetConstant(CN->getSExtValue(), DL, MVT::i64);
      return true;
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, MVT::i64);
  return true;
}

FunctionPass *llvm::createAlphaISelDag(AlphaTargetMachine &TM,
                                       CodeGenOptLevel OptLevel) {
  return new AlphaDAGToDAGISelLegacy(TM, OptLevel);
}
