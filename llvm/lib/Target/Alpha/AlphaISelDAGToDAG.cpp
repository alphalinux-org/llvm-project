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

  // Match any address as a single register operand.
  bool SelectAddrReg(SDValue Addr, SDValue &Reg) {
    Reg = Addr;
    return true;
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

  // Materialize a constant that does not fit in the 16-bit `lda` displacement
  // as an ldah/lda pair, when it decomposes into two 16-bit signed halves:
  // V = (Hi << 16) + Lo.  16-bit constants are handled by a pattern; constants
  // that need a third instruction (e.g. 0x7fffffff) are not supported yet.
  if (Node->getOpcode() == ISD::Constant && Node->getValueType(0) == MVT::i64) {
    int64_t V = cast<ConstantSDNode>(Node)->getSExtValue();
    int64_t Lo = static_cast<int16_t>(V);
    int64_t Hi = (V - Lo) >> 16;
    SDLoc DL(Node);
    if (!isInt<16>(V) && isInt<16>(Hi)) {
      SDValue Zero = CurDAG->getRegister(Alpha::R31, MVT::i64);
      SDNode *High = CurDAG->getMachineNode(
          Alpha::LDAH, DL, MVT::i64,
          CurDAG->getTargetConstant(Hi, DL, MVT::i64), Zero);
      SDNode *Low = CurDAG->getMachineNode(
          Alpha::LDA, DL, MVT::i64, CurDAG->getTargetConstant(Lo, DL, MVT::i64),
          SDValue(High, 0));
      ReplaceNode(Node, Low);
      return;
    }
    if (!isInt<16>(Hi)) {
      // Anything the ldah/lda pair cannot build goes in the constant pool and
      // is loaded GP-relative.  That is every constant wider than 32 bits, and
      // also those whose high half is 0x8000, which the signed field of ldah
      // cannot hold.
      CurDAG->getMachineFunction()
          .getInfo<AlphaMachineFunctionInfo>()
          ->setUsesGP();
      const Constant *CV =
          ConstantInt::get(Type::getInt64Ty(*CurDAG->getContext()), V);
      SDValue CPI = CurDAG->getTargetConstantPool(CV, MVT::i64, Align(8));
      SDValue GP = CurDAG->getRegister(Alpha::R29, MVT::i64);
      SDNode *High =
          CurDAG->getMachineNode(Alpha::LDAHg, DL, MVT::i64, CPI, GP);
      SDNode *Addr = CurDAG->getMachineNode(Alpha::LDAg, DL, MVT::i64, CPI,
                                            SDValue(High, 0));
      SDNode *Load =
          CurDAG->getMachineNode(Alpha::LDQ, DL, MVT::i64, SDValue(Addr, 0),
                                 CurDAG->getTargetConstant(0, DL, MVT::i64));
      ReplaceNode(Node, Load);
      return;
    }
  }

  SelectCode(Node);
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
