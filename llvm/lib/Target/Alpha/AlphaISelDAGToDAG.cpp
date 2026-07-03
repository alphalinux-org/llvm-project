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
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

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
