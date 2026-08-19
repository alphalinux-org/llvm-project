//===-- AlphaInstructionSelector.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the targeting of the InstructionSelector class for
// Alpha.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaRegisterBankInfo.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/CodeGen/GlobalISel/GIMatchTableExecutorImpl.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/Utils.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IntrinsicsAlpha.h"

#define DEBUG_TYPE "alpha-isel"

using namespace llvm;

#define GET_GLOBALISEL_PREDICATE_BITSET
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATE_BITSET

namespace {

class AlphaInstructionSelector : public InstructionSelector {
public:
  AlphaInstructionSelector(const AlphaTargetMachine &TM,
                           const AlphaSubtarget &STI,
                           const AlphaRegisterBankInfo &RBI);

  bool select(MachineInstr &I) override;
  static const char *getName() { return DEBUG_TYPE; }

private:
  bool selectImpl(MachineInstr &I, CodeGenCoverage &CoverageInfo) const;
  bool selectLoadStore(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectICmp(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectFConstant(MachineInstr &I, MachineRegisterInfo &MRI) const;

  const AlphaInstrInfo &TII;
  const AlphaRegisterInfo &TRI;
  const AlphaRegisterBankInfo &RBI;

#define GET_GLOBALISEL_PREDICATES_DECL
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATES_DECL

#define GET_GLOBALISEL_TEMPORARIES_DECL
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_TEMPORARIES_DECL
};

} // end anonymous namespace

#define GET_GLOBALISEL_IMPL
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_IMPL

AlphaInstructionSelector::AlphaInstructionSelector(
    const AlphaTargetMachine &TM, const AlphaSubtarget &STI,
    const AlphaRegisterBankInfo &RBI)
    : TII(*STI.getInstrInfo()), TRI(*STI.getRegisterInfo()), RBI(RBI),
#define GET_GLOBALISEL_PREDICATES_INIT
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATES_INIT
#define GET_GLOBALISEL_TEMPORARIES_INIT
#include "AlphaGenGlobalISel.inc"
#undef GET_GLOBALISEL_TEMPORARIES_INIT
{
}

// Which bank a register belongs to, whether it has been given a class already
// or still carries only a bank.
static bool isFPReg(Register Reg, const MachineRegisterInfo &MRI) {
  if (Reg.isPhysical())
    return Alpha::FPRCRegClass.contains(Reg);
  if (const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg))
    return Alpha::FPRCRegClass.hasSubClassEq(RC);
  const RegisterBank *RB = MRI.getRegBankOrNull(Reg);
  return RB && RB->getID() == Alpha::FPRRegBankID;
}

// A copy that survives selection still carries a register bank rather than a
// register class, which the register allocator cannot use.  Give it the class
// its bank stands for.
static bool selectCopy(MachineInstr &I, MachineRegisterInfo &MRI,
                       const RegisterBankInfo &RBI) {
  Register DstReg = I.getOperand(0).getReg();

  // A phi is not a copy of one register but of all of them, and every incoming
  // value has to end up in the same class as the result: phi elimination turns
  // the phi into a copy per incoming edge, and a copy whose two ends are in
  // different banks is one no instruction can perform.
  if (I.isPHI()) {
    bool DstFP = isFPReg(DstReg, MRI);
    const TargetRegisterClass &RC =
        DstFP ? Alpha::FPRCRegClass : Alpha::GPRCRegClass;
    MachineFunction &MF = *I.getParent()->getParent();
    const AlphaInstrInfo &TII =
        *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
    const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
    LLT Ty = MRI.getType(DstReg);
    bool Is32 = Ty.isValid() && Ty.getSizeInBits() == 32;

    if (DstReg.isVirtual() && !RBI.constrainGenericRegister(DstReg, RC, MRI))
      return false;

    for (unsigned Idx = 1; Idx < I.getNumOperands(); Idx += 2) {
      Register Reg = I.getOperand(Idx).getReg();
      if (!Reg.isVirtual())
        continue;
      // An incoming value that came from the other bank is moved across at the
      // end of the block it comes from, where the phi's copy will pick it up.
      if (isFPReg(Reg, MRI) != DstFP) {
        MachineBasicBlock &Pred = *I.getOperand(Idx + 1).getMBB();
        unsigned Opc = DstFP ? (Is32 ? Alpha::MOVi2f_S : Alpha::MOVi2f)
                             : (Is32 ? Alpha::MOVf2i_S : Alpha::MOVf2i);
        Register Tmp = MRI.createVirtualRegister(&RC);
        MachineInstrBuilder Move = BuildMI(Pred, Pred.getFirstTerminator(),
                                           I.getDebugLoc(), TII.get(Opc), Tmp)
                                       .addUse(Reg);
        constrainSelectedInstRegOperands(*Move, TII, TRI, RBI);
        I.getOperand(Idx).setReg(Tmp);
        continue;
      }
      if (!RBI.constrainGenericRegister(Reg, RC, MRI))
        return false;
    }
    return true;
  }

  Register SrcReg = I.getOperand(1).getReg();

  // There is no instruction that moves a value between an integer and a
  // floating register, so a copy across the two banks -- which is what passing
  // a float to a call in $f16 out of an integer register is -- has to go
  // through memory.  Hand it to the pseudo that does that; the same one the
  // SelectionDAG path uses, expanded once selection is finished.
  if (isFPReg(DstReg, MRI) != isFPReg(SrcReg, MRI)) {
    MachineBasicBlock &MBB = *I.getParent();
    MachineFunction &MF = *MBB.getParent();
    const AlphaInstrInfo &TII =
        *MF.getSubtarget<AlphaSubtarget>().getInstrInfo();
    bool ToFP = isFPReg(DstReg, MRI);

    // The width comes from whichever end is still generic; a 32-bit value moves
    // through the S_floating form, which converts the format as it goes.
    Register Typed = SrcReg.isVirtual() ? SrcReg : DstReg;
    LLT Ty = MRI.getType(Typed);
    bool Is32 = Ty.isValid() && Ty.getSizeInBits() == 32;

    unsigned Opc = ToFP ? (Is32 ? Alpha::MOVi2f_S : Alpha::MOVi2f)
                        : (Is32 ? Alpha::MOVf2i_S : Alpha::MOVf2i);
    const TargetRegisterClass &SrcRC =
        ToFP ? Alpha::GPRCRegClass : Alpha::FPRCRegClass;
    const TargetRegisterClass &TmpRC =
        ToFP ? Alpha::FPRCRegClass : Alpha::GPRCRegClass;

    if (SrcReg.isVirtual() && !MRI.getRegClassOrNull(SrcReg) &&
        !RBI.constrainGenericRegister(SrcReg, SrcRC, MRI))
      return false;

    Register Tmp = MRI.createVirtualRegister(&TmpRC);
    BuildMI(MBB, I, I.getDebugLoc(), TII.get(Opc), Tmp).addReg(SrcReg);
    I.getOperand(1).setReg(Tmp);
    SrcReg = Tmp;
  }

  // Copying into a physical register leaves nothing to name on that side, but
  // the value being copied out still has to be given a class of its own.
  if (SrcReg.isVirtual() && !MRI.getRegClassOrNull(SrcReg)) {
    const TargetRegisterClass &SrcRC =
        isFPReg(SrcReg, MRI) ? Alpha::FPRCRegClass : Alpha::GPRCRegClass;
    if (!RBI.constrainGenericRegister(SrcReg, SrcRC, MRI))
      return false;
  }

  if (DstReg.isPhysical())
    return true;

  if (MRI.getRegClassOrNull(DstReg))
    return true;

  const RegisterBank *RB = MRI.getRegBankOrNull(DstReg);
  if (!RB)
    return false;

  const TargetRegisterClass &RC = RB->getID() == Alpha::FPRRegBankID
                                      ? Alpha::FPRCRegClass
                                      : Alpha::GPRCRegClass;
  return RBI.constrainGenericRegister(DstReg, RC, MRI);
}

// A memory operand is a base register and a signed 16-bit displacement, which
// the address computation in front of the access can often be folded into.
bool AlphaInstructionSelector::selectLoadStore(MachineInstr &I,
                                               MachineRegisterInfo &MRI) const {
  bool IsLoad = I.getOpcode() == TargetOpcode::G_LOAD;
  Register ValReg = I.getOperand(0).getReg();
  Register AddrReg = I.getOperand(1).getReg();

  const MachineMemOperand &MMO = **I.memoperands_begin();
  uint64_t Size = MMO.getSizeInBits().getValue();

  // An access narrower than its own width is a misaligned one, and Alpha has no
  // instruction for it: the datum can straddle two quadwords, so it takes the
  // read-modify-write of both that the SelectionDAG path lowers it to.  Leave
  // it to that path rather than emit an access that would write only the part
  // of the value that fell in one quadword.
  if (MMO.getAlign().value() * 8 < Size)
    return false;

  const AlphaSubtarget &STI =
      I.getParent()->getParent()->getSubtarget<AlphaSubtarget>();
  bool IsFP = RBI.getRegBank(ValReg, MRI, TRI)->getID() == Alpha::FPRRegBankID;

  // Which narrow store to use without BWX.  The plain read-modify-write updates
  // one field of a quadword in place and is not atomic against another thread
  // writing a different field of the same quadword; -msafe-bwa asks for the
  // lock-based form instead.  RMW_STOREI8/16 carry Predicates = [UnsafeBWStore],
  // which is exactly this condition, so selecting one without checking it emits
  // an instruction whose own predicate says it must not appear.  The
  // SelectionDAG path asks the same question in AlphaISelDAGToDAG.
  bool UseSafeBWStore = STI.hasSafeBWA();

  unsigned Opc;
  switch (Size) {
  case 64:
    Opc = IsLoad ? (IsFP ? Alpha::LDT : Alpha::LDQ)
                 : (IsFP ? Alpha::STT : Alpha::STQ);
    break;
  case 32:
    Opc = IsLoad ? (IsFP ? Alpha::LDS : Alpha::LDL)
                 : (IsFP ? Alpha::STS : Alpha::STL);
    break;
  case 16:
  case 8:
    if (IsFP)
      return false;
    if (STI.hasBWX())
      Opc = IsLoad ? (Size == 8 ? Alpha::LDBU : Alpha::LDWU)
                   : (Size == 8 ? Alpha::STB : Alpha::STW);
    else if (IsLoad)
      Opc = Alpha::LDQ_U; // Followed by the extract below.
    else if (UseSafeBWStore)
      Opc = Size == 8 ? Alpha::SAFE_STOREI8 : Alpha::SAFE_STOREI16;
    else
      Opc = Size == 8 ? Alpha::RMW_STOREI8 : Alpha::RMW_STOREI16;
    break;
  default:
    return false;
  }

  // Without BWX a narrow access has no instruction of its own: a load reads
  // the quadword holding the datum and extracts it, and a store is one of the
  // two read-modify-write pseudos, both of which need scratch registers or a
  // custom inserter and cannot take a displacement.
  if (!STI.hasBWX() && Size < 32) {
    if (IsLoad) {
      Register Quad = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      MachineInstrBuilder Ld = BuildMI(*I.getParent(), I, I.getDebugLoc(),
                                       TII.get(Alpha::LDQ_U), Quad)
                                   .addUse(AddrReg);
      Ld.setMemRefs(I.memoperands());
      constrainSelectedInstRegOperands(*Ld, TII, TRI, RBI);
      MachineInstrBuilder Ext =
          BuildMI(*I.getParent(), I, I.getDebugLoc(),
                  TII.get(Size == 8 ? Alpha::EXTBL : Alpha::EXTWL), ValReg)
              .addUse(Quad)
              .addUse(AddrReg);
      I.eraseFromParent();
      constrainSelectedInstRegOperands(*Ext, TII, TRI, RBI);
      return true;
    }
    MachineFunction &MF = *I.getParent()->getParent();
    // Both forms read and write the whole quadword holding the field, so the
    // memory operand has to say so: carrying the original one- or two-byte
    // reference understates the footprint to anything that asks later whether
    // this store can alias another.  The SelectionDAG path widens it the same
    // way.
    auto Flags = (**I.memoperands_begin()).getFlags() |
                 MachineMemOperand::MOLoad | MachineMemOperand::MOStore;
    MachineMemOperand *Wide =
        MF.getMachineMemOperand(MachinePointerInfo(), Flags, 8, Align(8));

    if (UseSafeBWStore) {
      MachineInstrBuilder St =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Opc))
              .addUse(ValReg)
              .addUse(AddrReg);
      St.setMemRefs({Wide});
      I.eraseFromParent();
      constrainSelectedInstRegOperands(*St, TII, TRI, RBI);
      return true;
    }

    Register T1 = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
    Register T2 = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
    MachineInstrBuilder St =
        BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Opc))
            .addDef(T1, RegState::Dead)
            .addDef(T2, RegState::Dead)
            .addUse(ValReg)
            .addUse(AddrReg);
    St.setMemRefs({Wide});
    I.eraseFromParent();
    constrainSelectedInstRegOperands(*St, TII, TRI, RBI);
    return true;
  }

  // Fold a constant offset into the displacement when it fits.
  int64_t Disp = 0;
  MachineInstr *AddrDef = getDefIgnoringCopies(AddrReg, MRI);
  if (AddrDef && AddrDef->getOpcode() == TargetOpcode::G_PTR_ADD) {
    if (auto Offset =
            getIConstantVRegSExtVal(AddrDef->getOperand(2).getReg(), MRI)) {
      if (isInt<16>(*Offset)) {
        Disp = *Offset;
        AddrReg = AddrDef->getOperand(1).getReg();
      }
    }
  }

  MachineInstrBuilder MIB =
      BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Opc));
  if (IsLoad)
    MIB.addDef(ValReg);
  else
    MIB.addUse(ValReg);
  MIB.addUse(AddrReg).addImm(Disp);
  MIB.setMemRefs(I.memoperands());

  I.eraseFromParent();
  constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
  return true;
}

// Alpha compares a pair of registers and leaves 0 or 1 in a third.  There is no
// instruction for the greater-than forms or for inequality: the first are the
// less-than ones with the operands swapped, and the second is equality
// inverted.
bool AlphaInstructionSelector::selectICmp(MachineInstr &I,
                                          MachineRegisterInfo &MRI) const {
  auto Pred = static_cast<CmpInst::Predicate>(I.getOperand(1).getPredicate());
  Register Dst = I.getOperand(0).getReg();
  Register LHS = I.getOperand(2).getReg();
  Register RHS = I.getOperand(3).getReg();

  unsigned Opc;
  bool Swap = false;
  bool Invert = false;
  switch (Pred) {
  case CmpInst::ICMP_EQ:
    Opc = Alpha::CMPEQ;
    break;
  case CmpInst::ICMP_NE:
    Opc = Alpha::CMPEQ;
    Invert = true;
    break;
  case CmpInst::ICMP_SLT:
    Opc = Alpha::CMPLT;
    break;
  case CmpInst::ICMP_SLE:
    Opc = Alpha::CMPLE;
    break;
  case CmpInst::ICMP_SGT:
    Opc = Alpha::CMPLT;
    Swap = true;
    break;
  case CmpInst::ICMP_SGE:
    Opc = Alpha::CMPLE;
    Swap = true;
    break;
  case CmpInst::ICMP_ULT:
    Opc = Alpha::CMPULT;
    break;
  case CmpInst::ICMP_ULE:
    Opc = Alpha::CMPULE;
    break;
  case CmpInst::ICMP_UGT:
    Opc = Alpha::CMPULT;
    Swap = true;
    break;
  case CmpInst::ICMP_UGE:
    Opc = Alpha::CMPULE;
    Swap = true;
    break;
  default:
    return false;
  }

  if (Swap)
    std::swap(LHS, RHS);

  Register CmpDst = Dst;
  if (Invert)
    CmpDst = MRI.createVirtualRegister(&Alpha::GPRCRegClass);

  MachineInstrBuilder Cmp =
      BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Opc), CmpDst)
          .addUse(LHS)
          .addUse(RHS);
  constrainSelectedInstRegOperands(*Cmp, TII, TRI, RBI);

  if (Invert) {
    MachineInstrBuilder Xor =
        BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::XORi), Dst)
            .addUse(CmpDst)
            .addImm(1);
    constrainSelectedInstRegOperands(*Xor, TII, TRI, RBI);
  }

  I.eraseFromParent();
  return true;
}

// A floating-point constant lives in the constant pool, whose entries are
// local and so addressed from the global pointer: ldah !gprelhigh, then the
// load itself carries the !gprellow half.
bool AlphaInstructionSelector::selectFConstant(MachineInstr &I,
                                               MachineRegisterInfo &MRI) const {
  MachineFunction &MF = *I.getParent()->getParent();
  const ConstantFP *CFP = I.getOperand(1).getFPImm();
  LLT Ty = MRI.getType(I.getOperand(0).getReg());
  if (Ty != LLT::scalar(32) && Ty != LLT::scalar(64))
    return false;

  // Three values need no pool at all: $f31 reads as +0.0, its negation gives
  // -0.0, and an equal compare of it with itself gives exactly +2.0.  The bits
  // are the same for S_floating and T_floating, so the width does not matter.
  const APFloat &V = CFP->getValueAPF();
  unsigned CheapOpc = 0;
  if (V.isExactlyValue(+0.0))
    CheapOpc = Alpha::CPYS;
  else if (V.isExactlyValue(-0.0))
    CheapOpc = Alpha::CPYSN;
  else if (V.isExactlyValue(+2.0))
    CheapOpc = Alpha::CMPTEQ;
  if (CheapOpc) {
    MachineInstrBuilder MIB =
        BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(CheapOpc),
                I.getOperand(0).getReg())
            .addUse(Alpha::F31)
            .addUse(Alpha::F31);
    constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
    I.eraseFromParent();
    return true;
  }

  Align Alignment(Ty.getSizeInBytes());
  unsigned CPI = MF.getConstantPool()->getConstantPoolIndex(CFP, Alignment);
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  Register HighReg = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  MachineInstrBuilder High = BuildMI(*I.getParent(), I, I.getDebugLoc(),
                                     TII.get(Alpha::LDAHg), HighReg)
                                 .addConstantPoolIndex(CPI)
                                 .addUse(Alpha::R29);
  constrainSelectedInstRegOperands(*High, TII, TRI, RBI);

  MachineInstrBuilder Load =
      BuildMI(*I.getParent(), I, I.getDebugLoc(),
              TII.get(Ty == LLT::scalar(32) ? Alpha::LDSg : Alpha::LDTg),
              I.getOperand(0).getReg())
          .addConstantPoolIndex(CPI)
          .addUse(HighReg);
  constrainSelectedInstRegOperands(*Load, TII, TRI, RBI);

  I.eraseFromParent();
  return true;
}

bool AlphaInstructionSelector::select(MachineInstr &I) {
  MachineRegisterInfo &MRI = I.getParent()->getParent()->getRegInfo();

  if (!I.isPreISelOpcode()) {
    if (I.isCopy() || I.isPHI())
      return selectCopy(I, MRI, RBI);
    return true;
  }

  if (selectImpl(I, *CoverageInfo))
    return true;

  switch (I.getOpcode()) {
  case TargetOpcode::G_LOAD:
  case TargetOpcode::G_STORE:
    return selectLoadStore(I, MRI);
  case TargetOpcode::G_ICMP:
    return selectICmp(I, MRI);
  case TargetOpcode::G_FCONSTANT:
    return selectFConstant(I, MRI);
  case TargetOpcode::G_GLOBAL_VALUE: {
    // Small-data addressing is not selected here.
    if (I.getParent()
            ->getParent()
            ->getSubtarget<AlphaSubtarget>()
            .hasSmallData())
      return false;
    I.getParent()
        ->getParent()
        ->getInfo<AlphaMachineFunctionInfo>()
        ->setUsesGP();

    // A global the linker resolves itself is a fixed distance from the global
    // pointer, so its address is built with an ldah/lda pair rather than loaded
    // from the GOT.  Only a global the GOT entry has to answer for is loaded.
    const GlobalValue *GV = I.getOperand(1).getGlobal();
    MachineInstrBuilder MIB;
    if (isAlphaGprelAddressable(*GV)) {
      Register Hi = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      MachineInstrBuilder HiMIB =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::LDAHg), Hi)
              .add(I.getOperand(1))
              .addReg(Alpha::R29);
      MIB = BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::LDAg),
                    I.getOperand(0).getReg())
                .add(I.getOperand(1))
                .addReg(Hi);
      constrainSelectedInstRegOperands(*HiMIB, TII, TRI, RBI);
    } else {
      MIB = BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::LDQl),
                    I.getOperand(0).getReg())
                .add(I.getOperand(1));
    }
    I.eraseFromParent();
    constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
    return true;
  }
  case TargetOpcode::G_PTR_ADD: {
    // A pointer is just a quadword.
    I.setDesc(TII.get(Alpha::ADDQ));
    constrainSelectedInstRegOperands(I, TII, TRI, RBI);
    return true;
  }
  case TargetOpcode::G_INTTOPTR:
  case TargetOpcode::G_PTRTOINT: {
    I.setDesc(TII.get(TargetOpcode::COPY));
    return selectCopy(I, MRI, RBI);
  }
  case TargetOpcode::G_PHI: {
    // A phi keeps its own opcode; it only needs a register class.
    I.setDesc(TII.get(TargetOpcode::PHI));
    return selectCopy(I, MRI, RBI);
  }
  case TargetOpcode::G_ANYEXT:
  case TargetOpcode::G_TRUNC: {
    // A narrowing to a boolean has to discard the bits above the low one:
    // the legalizer widens boolean arithmetic to a quadword, so the value
    // being narrowed carries whatever those wider operations left behind.
    // Everything that consumes a boolean -- a branch, a conditional move, a
    // sign extension -- reads the whole register, and would read that debris
    // as part of the condition.
    if (I.getOpcode() == TargetOpcode::G_TRUNC &&
        MRI.getType(I.getOperand(0).getReg()) == LLT::scalar(1)) {
      MachineInstrBuilder MIB =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::ANDi),
                  I.getOperand(0).getReg())
              .addUse(I.getOperand(1).getReg())
              .addImm(1);
      I.eraseFromParent();
      constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
      return true;
    }
    // Otherwise every value already occupies a whole register, so a widening
    // or narrowing that does not change the bits is a copy.
    I.setDesc(TII.get(TargetOpcode::COPY));
    return selectCopy(I, MRI, RBI);
  }
  case TargetOpcode::G_ZEXT: {
    // zapnot keeps the bytes its mask names and zeroes the rest; a boolean
    // needs only its low bit.
    LLT SrcTy = MRI.getType(I.getOperand(1).getReg());
    unsigned Imm;
    if (SrcTy == LLT::scalar(1))
      Imm = 0; // Handled with an and below.
    else if (SrcTy == LLT::scalar(8))
      Imm = 1;
    else if (SrcTy == LLT::scalar(16))
      Imm = 3;
    else if (SrcTy == LLT::scalar(32))
      Imm = 15;
    else
      return false;

    MachineInstrBuilder MIB =
        BuildMI(*I.getParent(), I, I.getDebugLoc(),
                TII.get(Imm ? Alpha::ZAPNOTi : Alpha::ANDi),
                I.getOperand(0).getReg())
            .addUse(I.getOperand(1).getReg())
            .addImm(Imm ? Imm : 1);
    I.eraseFromParent();
    constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
    return true;
  }
  case TargetOpcode::G_SEXT: {
    // addl sign-extends a longword; a byte or a word is extended with a pair
    // of shifts, or with the BWX instruction where there is one.
    LLT SrcTy = MRI.getType(I.getOperand(1).getReg());
    Register Dst = I.getOperand(0).getReg();
    Register Src = I.getOperand(1).getReg();
    MachineInstrBuilder MIB;
    if (SrcTy == LLT::scalar(32)) {
      MIB =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::ADDL), Dst)
              .addUse(Src)
              .addUse(Alpha::R31);
    } else if (SrcTy == LLT::scalar(8) || SrcTy == LLT::scalar(16)) {
      unsigned Shift = SrcTy == LLT::scalar(8) ? 56 : 48;
      Register Tmp = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      MachineInstrBuilder Sll =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::SLLi), Tmp)
              .addUse(Src)
              .addImm(Shift);
      constrainSelectedInstRegOperands(*Sll, TII, TRI, RBI);
      MIB =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::SRAi), Dst)
              .addUse(Tmp)
              .addImm(Shift);
    } else if (SrcTy == LLT::scalar(1)) {
      // A boolean holds 0 or 1; negating it gives 0 or -1.
      MIB =
          BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::SUBQ), Dst)
              .addUse(Alpha::R31)
              .addUse(Src);
    } else {
      return false;
    }
    I.eraseFromParent();
    constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
    return true;
  }
  case TargetOpcode::G_BRCOND: {
    // A branch tests the whole register: the condition holds 0 or 1.
    MachineInstrBuilder MIB =
        BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::BNE))
            .addUse(I.getOperand(0).getReg())
            .addMBB(I.getOperand(1).getMBB());
    I.eraseFromParent();
    constrainSelectedInstRegOperands(*MIB, TII, TRI, RBI);
    return true;
  }
  case TargetOpcode::G_FRAME_INDEX: {
    // The address of a stack slot: an lda whose displacement the frame index
    // elimination fills in.
    I.setDesc(TII.get(Alpha::LEA));
    I.addOperand(MachineOperand::CreateImm(0));
    constrainSelectedInstRegOperands(I, TII, TRI, RBI);
    return true;
  }
  default:
    return false;
  }
}

namespace llvm {
InstructionSelector *
createAlphaInstructionSelector(const AlphaTargetMachine &TM,
                               const AlphaSubtarget &STI,
                               const AlphaRegisterBankInfo &RBI) {
  return new AlphaInstructionSelector(TM, STI, RBI);
}
} // end namespace llvm
