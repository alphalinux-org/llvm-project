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

  // Add a 32-bit signed value to Base with ldah/lda (Base + (Hi << 16) + Lo).
  SDValue buildConstant32(int32_t V32, SDValue Base, const SDLoc &DL);

  // Materialize a 64-bit constant entirely in code (no constant pool).
  SDNode *buildConstantInline(int64_t V, const SDLoc &DL);

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
    if (!isInt<16>(V) && isInt<32>(V)) {
      SDValue Zero = CurDAG->getRegister(Alpha::R31, MVT::i64);
      ReplaceNode(Node,
                  buildConstant32(static_cast<int32_t>(V), Zero, DL).getNode());
      return;
    }
    if (!isInt<32>(V)) {
      // Build a wide constant inline (ldah/lda of each 32-bit half combined
      // with a shift) when the constant pool must be avoided; otherwise place
      // it in the pool and load it GP-relative.
      if (Subtarget->hasBuildConstants()) {
        ReplaceNode(Node, buildConstantInline(V, DL));
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

SDValue AlphaDAGToDAGISel::buildConstant32(int32_t V32, SDValue Base,
                                           const SDLoc &DL) {
  // Add a 32-bit signed value to Base with ldah/lda: Base + (Hi << 16) + Lo,
  // where V32 = (Hi << 16) + Lo.  Hi lies in [-0x8000, 0x8000]; the +0x8000
  // case does not fit ldah's signed field and is emitted as two ldah halves.
  // Every caller passes a non-zero value, so at least one of the two halves
  // below emits an instruction and the result is always a new node rather than
  // Base itself.
  assert(V32 != 0 && "buildConstant32 of zero would hand back Base unchanged");
  int64_t Lo = static_cast<int16_t>(V32);
  int64_t Hi = (static_cast<int64_t>(V32) - Lo) >> 16;
  SDValue Cur = Base;
  if (Hi != 0) {
    if (isInt<16>(Hi)) {
      Cur = SDValue(CurDAG->getMachineNode(
                        Alpha::LDAH, DL, MVT::i64,
                        CurDAG->getTargetConstant(Hi, DL, MVT::i64), Cur),
                    0);
    } else {
      // Hi == 0x8000: split into two ldah of 0x4000.
      for (int I = 0; I < 2; ++I)
        Cur = SDValue(CurDAG->getMachineNode(
                          Alpha::LDAH, DL, MVT::i64,
                          CurDAG->getTargetConstant(Hi / 2, DL, MVT::i64), Cur),
                      0);
    }
  }
  if (Lo != 0)
    Cur = SDValue(CurDAG->getMachineNode(
                      Alpha::LDA, DL, MVT::i64,
                      CurDAG->getTargetConstant(Lo, DL, MVT::i64), Cur),
                  0);
  return Cur;
}

SDNode *AlphaDAGToDAGISel::buildConstantInline(int64_t V, const SDLoc &DL) {
  SDValue Zero = CurDAG->getRegister(Alpha::R31, MVT::i64);
  // The subtraction is done unsigned: V - Lo32 overflows a signed 64-bit value
  // for a V near INT64_MAX whose low half is negative, and the wrapped result
  // is the one wanted.
  uint64_t UV = static_cast<uint64_t>(V);
  int32_t Lo32 = static_cast<int32_t>(UV);
  int32_t Hi32 =
      static_cast<int32_t>((UV - static_cast<uint64_t>(int64_t(Lo32))) >> 32);

  // V = (Hi32 << 32) + Lo32.  Build the (adjusted) high half, shift it up, then
  // add the low half; the sign of Lo32 is already accounted for in Hi32.
  SDValue High = buildConstant32(Hi32, Zero, DL);
  SDValue Shifted = SDValue(
      CurDAG->getMachineNode(Alpha::SLLi, DL, MVT::i64, High,
                             CurDAG->getTargetConstant(32, DL, MVT::i64)),
      0);
  if (Lo32 == 0)
    return Shifted.getNode();
  return buildConstant32(Lo32, Shifted, DL).getNode();
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
