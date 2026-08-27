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
  bool selectInstr(MachineInstr &I) const;
  static const char *getName() { return DEBUG_TYPE; }

private:
  bool selectImpl(MachineInstr &I, CodeGenCoverage &CoverageInfo) const;
  bool selectLoadStore(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectMisalignedLoadStore(MachineInstr &I, MachineRegisterInfo &MRI,
                                 bool IsLoad, bool IsFP, uint64_t Size) const;
  bool selectExtLoad(MachineInstr &I, MachineRegisterInfo &MRI,
                     uint64_t Size) const;
  bool selectICmp(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectFCmp(MachineInstr &I, MachineRegisterInfo &MRI) const;
  Register emitFCmpBit(MachineInstr &I, MachineRegisterInfo &MRI,
                       unsigned Opc, Register LHS, Register RHS,
                       Register Dst = Register()) const;
  bool selectIntFPConv(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectConstant(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectSelect(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectFConstant(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectBrJT(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectGprelAddress(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectAluImm(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectNarrowArith(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectFieldExtract(MachineInstr &I, MachineRegisterInfo &MRI) const;
  bool selectBrCond(MachineInstr &I, MachineRegisterInfo &MRI) const;

  // Build an instruction in front of I, and remember it: everything built this
  // way has its register operands constrained to real classes once selection of
  // I succeeds.  Tying the two together removes the one mistake whose symptom
  // appears nowhere near its cause -- a missing constrain leaves a selected
  // instruction carrying a generic register class, and the register allocator
  // is what finally objects.
  MachineInstrBuilder emit(MachineInstr &I, unsigned Opc,
                           Register Def = Register()) const;
  // The same, for the few places that build somewhere other than in front of
  // I.
  MachineInstrBuilder emitAt(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator It, const DebugLoc &DL,
                             unsigned Opc, Register Def = Register()) const;

  // The high half of a gp-relative address: ldah Hi, sym($29) !gprelhigh,
  // which is also what makes the function need a global pointer.  The low half
  // is folded into whichever instruction uses the address, so it is left to
  // the caller when that instruction is not a plain lda or load.
  Register emitGprelHigh(MachineInstr &I, const MachineOperand &Sym,
                         MachineRegisterInfo &MRI) const;
  // Both halves, where the low one is an instruction of its own.
  MachineInstrBuilder emitGprelPair(MachineInstr &I, unsigned LoOpc,
                                    Register Def, const MachineOperand &Sym,
                                    MachineRegisterInfo &MRI) const;

  // What select() has built for the instruction it is selecting.
  mutable SmallVector<MachineInstr *, 8> ToConstrain;

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

MachineInstrBuilder AlphaInstructionSelector::emitAt(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator It, const DebugLoc &DL,
    unsigned Opc, Register Def) const {
  MachineInstrBuilder MIB = Def ? BuildMI(MBB, It, DL, TII.get(Opc), Def)
                                : BuildMI(MBB, It, DL, TII.get(Opc));
  ToConstrain.push_back(MIB.getInstr());
  return MIB;
}

MachineInstrBuilder AlphaInstructionSelector::emit(MachineInstr &I,
                                                   unsigned Opc,
                                                   Register Def) const {
  return emitAt(*I.getParent(), I, I.getDebugLoc(), Opc, Def);
}

Register
AlphaInstructionSelector::emitGprelHigh(MachineInstr &I,
                                        const MachineOperand &Sym,
                                        MachineRegisterInfo &MRI) const {
  I.getParent()->getParent()->getInfo<AlphaMachineFunctionInfo>()->setUsesGP();
  Register Hi = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  emit(I, Alpha::LDAHg, Hi).add(Sym).addUse(Alpha::R29);
  return Hi;
}

MachineInstrBuilder
AlphaInstructionSelector::emitGprelPair(MachineInstr &I, unsigned LoOpc,
                                        Register Def, const MachineOperand &Sym,
                                        MachineRegisterInfo &MRI) const {
  Register Hi = emitGprelHigh(I, Sym, MRI);
  return emit(I, LoOpc, Def).add(Sym).addUse(Hi);
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
  bool IsLoad = I.getOpcode() != TargetOpcode::G_STORE;
  Register ValReg = I.getOperand(0).getReg();
  Register AddrReg = I.getOperand(1).getReg();

  const MachineMemOperand &MMO = **I.memoperands_begin();
  uint64_t Size = MMO.getSizeInBits().getValue();

  // An extending load reads the bytes a plain load of the same width reads;
  // what differs is what fills the register above them.  Give it the extension
  // it needs, if it needs one at all, and then select it as that plain load.
  if (I.getOpcode() == TargetOpcode::G_SEXTLOAD ||
      I.getOpcode() == TargetOpcode::G_ZEXTLOAD)
    return selectExtLoad(I, MRI, Size);

  const AlphaSubtarget &STI =
      I.getParent()->getParent()->getSubtarget<AlphaSubtarget>();
  bool IsFP = RBI.getRegBank(ValReg, MRI, TRI)->getID() == Alpha::FPRRegBankID;

  // An access narrower than its own width is a misaligned one, and Alpha has no
  // instruction for it: the datum can straddle two quadwords, so it takes both.
  if (MMO.getAlign().value() * 8 < Size)
    return selectMisalignedLoadStore(I, MRI, IsLoad, IsFP, Size);

  // Which narrow store to use without BWX.  The plain read-modify-write updates
  // one field of a quadword in place and is not atomic against another thread
  // writing a different field of the same quadword; -msafe-bwa asks for the
  // lock-based form instead.  RMW_STOREI8/16 carry Predicates =
  // [UnsafeBWStore], which is exactly this condition, so selecting one without
  // checking it emits an instruction whose own predicate says it must not
  // appear.  The SelectionDAG path asks the same question in AlphaISelDAGToDAG.
  //
  // An atomic narrow store takes the lock-based form whatever the feature says.
  // -msafe-bwa is about a *plain* store to a neighbouring field, which the
  // program is allowed to assume is a separate object; an atomic store makes
  // that assumption unconditionally, so the read-modify-write would lose an
  // update to another byte of the same quadword no matter how the module was
  // compiled.  AlphaInstrInfo.td states the same rule for the DAG path, whose
  // atomic_store_8/16 patterns name SAFE_STOREI8/16 outright.
  bool UseSafeBWStore = STI.hasSafeBWA() || MMO.isAtomic();

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
      MachineInstrBuilder Ld = emit(I, Alpha::LDQ_U, Quad).addUse(AddrReg);
      Ld.setMemRefs(I.memoperands());
      emit(I, Size == 8 ? Alpha::EXTBL : Alpha::EXTWL, ValReg)
          .addUse(Quad)
          .addUse(AddrReg);
      I.eraseFromParent();
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
      MachineInstrBuilder St = emit(I, Opc).addUse(ValReg).addUse(AddrReg);
      St.setMemRefs({Wide});
      I.eraseFromParent();
      return true;
    }

    Register T1 = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
    Register T2 = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
    MachineInstrBuilder St = emit(I, Opc)
                                 .addDef(T1, RegState::Dead)
                                 .addDef(T2, RegState::Dead)
                                 .addUse(ValReg)
                                 .addUse(AddrReg);
    St.setMemRefs({Wide});
    I.eraseFromParent();
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

  // A gp-relative global address is an ldah/lda pair, and the lda -- the
  // !gprellow half -- is a displacement the access can carry itself.  Folding
  // it in leaves only the ldah, which is what the SelectionDAG path's
  // GprelFold patterns do; without it every such load is one instruction
  // longer than the same load on the other path.
  if (Disp == 0 && AddrDef &&
      AddrDef->getOpcode() == TargetOpcode::G_GLOBAL_VALUE &&
      MRI.hasOneNonDBGUse(AddrReg) && (Size == 32 || Size == 64) &&
      !STI.hasSmallData()) {
    const GlobalValue &GV = *AddrDef->getOperand(1).getGlobal();
    if (!GV.isThreadLocal() && isAlphaGprelAddressable(GV)) {
      unsigned GOpc;
      if (Size == 64)
        GOpc = IsLoad ? (IsFP ? Alpha::LDTg : Alpha::LDQg)
                      : (IsFP ? Alpha::STTg : Alpha::STQg);
      else
        GOpc = IsLoad ? (IsFP ? Alpha::LDSg : Alpha::LDLg)
                      : (IsFP ? Alpha::STSg : Alpha::STLg);

      Register Hi = emitGprelHigh(I, AddrDef->getOperand(1), MRI);
      MachineInstrBuilder GMIB = emit(I, GOpc);
      if (IsLoad)
        GMIB.addDef(ValReg);
      else
        GMIB.addUse(ValReg);
      GMIB.add(AddrDef->getOperand(1)).addUse(Hi);
      GMIB.setMemRefs(I.memoperands());

      I.eraseFromParent();
      return true;
    }
  }

  MachineInstrBuilder MIB = emit(I, Opc);
  if (IsLoad)
    MIB.addDef(ValReg);
  else
    MIB.addUse(ValReg);
  MIB.addUse(AddrReg).addImm(Disp);
  MIB.setMemRefs(I.memoperands());

  I.eraseFromParent();
  return true;
}

// An extending load: G_SEXTLOAD or G_ZEXTLOAD, whose memory type is narrower
// than the register it lands in.  Nothing here reads memory -- the load itself
// goes back through selectLoadStore as a plain one -- and all this decides is
// what has to happen to the bits above the field afterwards.
//
// Which extension is free depends on the load: ldl sign-extends the longword
// it reads, while ldbu and ldwu zero-extend, the pre-BWX extract zero-extends
// what it extracts, and so does the misaligned expansion.  So exactly one of
// the two extensions is already done by the time the value is in a register,
// and the other one is a single instruction.
bool AlphaInstructionSelector::selectExtLoad(MachineInstr &I,
                                             MachineRegisterInfo &MRI,
                                             uint64_t Size) const {
  bool IsSigned = I.getOpcode() == TargetOpcode::G_SEXTLOAD;
  if (Size != 8 && Size != 16 && Size != 32)
    return false;

  const MachineMemOperand &MMO = **I.memoperands_begin();
  bool Misaligned = MMO.getAlign().value() * 8 < Size;
  bool LoadSignFills = Size == 32 && !Misaligned;

  if (IsSigned != LoadSignFills) {
    MachineBasicBlock &MBB = *I.getParent();
    const AlphaSubtarget &STI = MBB.getParent()->getSubtarget<AlphaSubtarget>();
    Register DstReg = I.getOperand(0).getReg();
    // The load writes this instead, and the extension below reads it.  It is
    // built generic and given a bank rather than a class: the two instructions
    // that touch it are constrained when they are selected, and each of them
    // is the thing that knows which class it needs.
    Register RawReg = MRI.createGenericVirtualRegister(LLT::scalar(64));
    MRI.setRegBank(RawReg, RBI.getRegBank(Alpha::GPRRegBankID));

    // After the load, which is still this instruction: it is selected below,
    // and whatever it becomes is built in front of it and takes its place.
    auto Ext = std::next(I.getIterator());
    const DebugLoc &DL = I.getDebugLoc();
    auto Build = [&](unsigned Opc, Register Dst) {
      return emitAt(MBB, Ext, DL, Opc, Dst);
    };

    MachineInstrBuilder Last;
    if (!IsSigned) {
      // Sign-filled and wanted zero-filled, which is the ldl case alone:
      // keep the four bytes the load read and zero the rest.
      Last = Build(Alpha::ZAPNOTi, DstReg).addUse(RawReg).addImm(0xf);
    } else if (Size == 32) {
      // addl sign-extends its longword result, and $31 adds nothing to it.
      Last = Build(Alpha::ADDL, DstReg).addUse(RawReg).addUse(Alpha::R31);
    } else if (STI.hasBWX()) {
      Last =
          Build(Size == 8 ? Alpha::SEXTB : Alpha::SEXTW, DstReg).addUse(RawReg);
    } else {
      // Shift the field up to the top of the register and back down with an
      // arithmetic shift, which fills from its sign bit.
      unsigned Shift = 64 - Size;
      Register Up = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      Build(Alpha::SLLi, Up).addUse(RawReg).addImm(Shift);
      Last = Build(Alpha::SRAi, DstReg).addUse(Up).addImm(Shift);
    }
    I.getOperand(0).setReg(RawReg);
  }

  I.setDesc(TII.get(TargetOpcode::G_LOAD));
  return selectLoadStore(I, MRI);
}

// A misaligned access, expanded the way AlphaTargetLowering::LowerLOAD and
// LowerSTORE expand it on the SelectionDAG path.  The datum falls in one
// quadword or straddles two, and which it is is not known until the address is:
// a load therefore reads both quadwords the datum can fall in, extracts the
// part of the field each holds, and splices the two halves together, and a
// store goes to the read-modify-write pseudo, which needs scratch registers and
// a bundle of its own and so is expanded later rather than written out here.
//
// Only two, four and eight bytes have extract instructions of the right width.
// A one-byte access is aligned by construction and never arrives here.
bool AlphaInstructionSelector::selectMisalignedLoadStore(
    MachineInstr &I, MachineRegisterInfo &MRI, bool IsLoad, bool IsFP,
    uint64_t Size) const {
  unsigned Bytes = Size / 8;
  if (Bytes != 2 && Bytes != 4 && Bytes != 8)
    return false;
  // The floating registers cannot be extracted from or inserted into; a
  // misaligned floating access moves through an integer register, in the same
  // S_floating form the aligned narrow move uses.
  if (IsFP && Bytes == 2)
    return false;

  MachineBasicBlock &MBB = *I.getParent();
  MachineFunction &MF = *MBB.getParent();
  const AlphaSubtarget &STI = MF.getSubtarget<AlphaSubtarget>();
  Register ValReg = I.getOperand(0).getReg();
  Register AddrReg = I.getOperand(1).getReg();
  const MachineMemOperand &MMO = **I.memoperands_begin();

  auto NewGPR = [&] { return MRI.createVirtualRegister(&Alpha::GPRCRegClass); };
  if (IsLoad) {
    // Each ldq_u reads the aligned quadword holding its address, so neither
    // touches the bytes the load names and both are described as a quadword at
    // an unknown address -- as the SelectionDAG path describes them.
    auto Flags = (MMO.getFlags() & MachineMemOperand::MOVolatile) |
                 MachineMemOperand::MOLoad;
    auto WideMMO = [&] {
      return MF.getMachineMemOperand(MachinePointerInfo(), Flags, 8, Align(8));
    };

    Alpha::FieldOps Ops = Alpha::getFieldOps(Bytes);
    unsigned ExtL = Ops.ExtL, ExtH = Ops.ExtH;

    Register Lo = NewGPR();
    MachineInstrBuilder LdLo = emit(I, Alpha::LDQ_U, Lo).addUse(AddrReg);
    LdLo.setMemRefs({WideMMO()});

    // The last byte of the field, which is in the second quadword exactly when
    // the field straddles the boundary; when it does not, this reads the same
    // quadword again and extqh contributes nothing.
    Register AddrHi = NewGPR();
    emit(I, Alpha::LDA, AddrHi).addImm(Bytes - 1).addUse(AddrReg);
    Register Hi = NewGPR();
    MachineInstrBuilder LdHi = emit(I, Alpha::LDQ_U, Hi).addUse(AddrHi);
    LdHi.setMemRefs({WideMMO()});

    // Both extracts take the *original* address: it is what says where in the
    // quadword pair the field begins.
    Register PartL = NewGPR();
    emit(I, ExtL, PartL).addUse(Lo).addUse(AddrReg);
    Register PartH = NewGPR();
    emit(I, ExtH, PartH).addUse(Hi).addUse(AddrReg);

    Register Whole = IsFP ? NewGPR() : ValReg;
    emit(I, Alpha::BIS, Whole).addUse(PartL).addUse(PartH);

    if (IsFP)
      emit(I, Bytes == 4 ? Alpha::MOVi2f_S : Alpha::MOVi2f, ValReg)
          .addUse(Whole);

    I.eraseFromParent();
    return true;
  }

  Register Val = ValReg;
  MachineInstrBuilder Mov;
  if (IsFP) {
    Val = NewGPR();
    Mov = emit(I, Bytes == 4 ? Alpha::MOVf2i_S : Alpha::MOVf2i, Val)
              .addUse(ValReg);
  }

  // With -msafe-partial each spanned quadword is updated by a lock-based loop
  // instead, so that the read-modify-write is atomic against a thread writing
  // another field of the same quadword.  The two pseudos differ in that and in
  // nothing else; the plain one needs four scratch registers, the lock-based
  // one gets its own inside its inserter.
  MachineInstrBuilder St;
  if (STI.hasSafePartial()) {
    St = emit(I, Alpha::SAFE_USTORE);
  } else {
    St = emit(I, Alpha::RMW_USTORE);
    for (unsigned N = 0; N != 4; ++N)
      St.addDef(NewGPR(), RegState::Dead);
  }
  St.addUse(Val).addUse(AddrReg).addImm(Bytes);
  St.setMemRefs(I.memoperands());

  I.eraseFromParent();
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

  emit(I, Opc, CmpDst).addUse(LHS).addUse(RHS);

  if (Invert) {
    emit(I, Alpha::XORi, Dst).addUse(CmpDst).addImm(1);
  }

  I.eraseFromParent();
  return true;
}

// A floating compare leaves 2.0 or 0.0 in a floating register, so the answer is
// the bits of that moved into an integer register and shifted right by 62.
Register AlphaInstructionSelector::emitFCmpBit(MachineInstr &I,
                                               MachineRegisterInfo &MRI,
                                               unsigned Opc, Register LHS,
                                               Register RHS,
                                               Register Dst) const {
  Register FPRes = MRI.createVirtualRegister(&Alpha::FPRCRegClass);
  emit(I, Opc, FPRes).addUse(LHS).addUse(RHS);

  // FCMPRES, not a move and a shift: without the FIX extension there is no
  // integer/floating move, and spelling one out here would send the result
  // through the bitcast stack slot and put a frame on a leaf function.  Its
  // custom inserter picks ftoit plus the shift, or a branch on the floating
  // condition, according to the subtarget.
  Register Res = Dst ? Dst : MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  emit(I, Alpha::FCMPRES, Res).addUse(FPRes);
  return Res;
}

// There are only the equal, less-than and less-or-equal instructions: the
// greater forms swap the operands, and a condition that admits an unordered
// pair is its ordered opposite inverted.  ord and uno, and the two conditions
// that separate equality from orderedness, need two compares combined.
//
// None of this holds a NaN at arm's length: cmpteq, cmptlt and cmptle signal
// an invalid operation for one unless they carry the /su qualifier, which
// -mieee supplies and the trap-mode machinery attaches.  What the sequences
// below rely on is only that an ordered compare answers false for a NaN, which
// is also what the SelectionDAG path expands ord and uno into.
bool AlphaInstructionSelector::selectFCmp(MachineInstr &I,
                                          MachineRegisterInfo &MRI) const {
  auto Pred = static_cast<CmpInst::Predicate>(I.getOperand(1).getPredicate());
  Register Dst = I.getOperand(0).getReg();
  Register LHS = I.getOperand(2).getReg();
  Register RHS = I.getOperand(3).getReg();

  // ord(a,b) is a == a && b == b; uno is its inverse.  one(a,b) is a < b ||
  // b < a, which is false for a NaN, and ueq is its inverse.
  unsigned CombineOpc = 0;
  bool CombineInvert = false;
  switch (Pred) {
  case CmpInst::FCMP_ORD:
    CombineOpc = Alpha::AND;
    break;
  case CmpInst::FCMP_UNO:
    CombineOpc = Alpha::AND;
    CombineInvert = true;
    break;
  case CmpInst::FCMP_ONE:
    CombineOpc = Alpha::BIS;
    break;
  case CmpInst::FCMP_UEQ:
    CombineOpc = Alpha::BIS;
    CombineInvert = true;
    break;
  default:
    break;
  }

  if (CombineOpc) {
    bool IsOrdered = CombineOpc == Alpha::AND;
    Register A = emitFCmpBit(I, MRI, IsOrdered ? Alpha::CMPTEQ : Alpha::CMPTLT,
                             LHS, IsOrdered ? LHS : RHS);
    Register B = emitFCmpBit(I, MRI, IsOrdered ? Alpha::CMPTEQ : Alpha::CMPTLT,
                             RHS, IsOrdered ? RHS : LHS);
    Register CombineDst =
        CombineInvert ? MRI.createVirtualRegister(&Alpha::GPRCRegClass) : Dst;
    emit(I, CombineOpc, CombineDst).addUse(A).addUse(B);
    if (CombineInvert) {
      emit(I, Alpha::XORi, Dst).addUse(CombineDst).addImm(1);
    }
    I.eraseFromParent();
    return true;
  }

  unsigned Opc;
  bool Swap = false;
  bool Invert = false;
  switch (Pred) {
  case CmpInst::FCMP_OEQ:
    Opc = Alpha::CMPTEQ;
    break;
  case CmpInst::FCMP_UNE:
    Opc = Alpha::CMPTEQ;
    Invert = true;
    break;
  case CmpInst::FCMP_OLT:
    Opc = Alpha::CMPTLT;
    break;
  case CmpInst::FCMP_OLE:
    Opc = Alpha::CMPTLE;
    break;
  case CmpInst::FCMP_OGT:
    Opc = Alpha::CMPTLT;
    Swap = true;
    break;
  case CmpInst::FCMP_OGE:
    Opc = Alpha::CMPTLE;
    Swap = true;
    break;
  case CmpInst::FCMP_UGE:
    Opc = Alpha::CMPTLT;
    Invert = true;
    break;
  case CmpInst::FCMP_UGT:
    Opc = Alpha::CMPTLE;
    Invert = true;
    break;
  case CmpInst::FCMP_ULT:
    Opc = Alpha::CMPTLE;
    Swap = true;
    Invert = true;
    break;
  case CmpInst::FCMP_ULE:
    Opc = Alpha::CMPTLT;
    Swap = true;
    Invert = true;
    break;
  default:
    // FCMP_TRUE and FCMP_FALSE do not reach instruction selection.
    return false;
  }

  if (Swap)
    std::swap(LHS, RHS);

  Register Bit = emitFCmpBit(I, MRI, Opc, LHS, RHS, Invert ? Register() : Dst);
  if (Invert) {
    emit(I, Alpha::XORi, Dst).addUse(Bit).addImm(1);
  }

  I.eraseFromParent();
  return true;
}

// cmovne leaves its destination alone when the condition is zero, so a select
// is the false value in the destination and a conditional move of the true one
// over it.
bool AlphaInstructionSelector::selectSelect(MachineInstr &I,
                                            MachineRegisterInfo &MRI) const {
  Register Dst = I.getOperand(0).getReg();
  Register Cond = I.getOperand(1).getReg();
  Register True = I.getOperand(2).getReg();
  Register False = I.getOperand(3).getReg();

  // Choosing between two floating values is fcmovne, which tests a floating
  // register against zero; the 0/1 condition is moved across as it is, its bits
  // being zero or not zero either way.
  if (RBI.getRegBank(Dst, MRI, TRI)->getID() == Alpha::FPRRegBankID) {
    Register Moved = MRI.createVirtualRegister(&Alpha::FPRCRegClass);
    emit(I, Alpha::MOVi2f, Moved).addUse(Cond);

    emit(I, Alpha::FCMOVNE, Dst).addUse(False).addUse(Moved).addUse(True);

    I.eraseFromParent();
    return true;
  }

  emit(I, Alpha::CMOVNE, Dst).addUse(False).addUse(Cond).addUse(True);

  I.eraseFromParent();
  return true;
}

// lda carries a signed 16-bit displacement and ldah the same shifted left 16,
// so a constant that fits in 32 bits is built from a pair of them; anything
// wider goes in the constant pool.  A pattern covers the 16-bit case already.
bool AlphaInstructionSelector::selectConstant(MachineInstr &I,
                                              MachineRegisterInfo &MRI) const {
  Register Dst = I.getOperand(0).getReg();
  int64_t V = I.getOperand(1).getCImm()->getSExtValue();
  MachineBasicBlock &MBB = *I.getParent();
  MachineFunction &MF = *MBB.getParent();
  const AlphaSubtarget &STI = MF.getSubtarget<AlphaSubtarget>();

  // Emit a constant-materialization sequence, chaining the steps through fresh
  // virtual registers so that only the last one writes Out.
  auto emitSteps = [&](ArrayRef<Alpha::ConstantStep> Steps, Register Base,
                       Register Out) {
    Register Cur = Base;
    for (auto [N, S] : enumerate(Steps)) {
      Register Next = N + 1 == Steps.size()
                          ? Out
                          : MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      MachineInstrBuilder MIB = emit(I, S.Opc, Next);
      // sll takes its register operand first, ldah and lda their displacement.
      if (S.Opc == Alpha::SLLi)
        MIB.addUse(Cur).addImm(S.Imm);
      else
        MIB.addImm(S.Imm).addUse(Cur);
      Cur = Next;
    }
  };

  SmallVector<Alpha::ConstantStep, 8> Steps;
  if (isInt<32>(V)) {
    Alpha::buildConstant32Steps(static_cast<int32_t>(V), Steps);
    emitSteps(Steps, Alpha::R31, Dst);
    I.eraseFromParent();
    return true;
  }

  // Wider than 32 bits.  -mbuild-constants asks for it to be built inline
  // rather than fetched from the constant pool, because reaching the pool needs
  // a global pointer: the dynamic loader runs before its own is established.
  // The SelectionDAG path asks the same question in AlphaISelDAGToDAG.
  if (STI.hasBuildConstants()) {
    Alpha::buildConstantSteps(V, Steps);
    emitSteps(Steps, Alpha::R31, Dst);
    I.eraseFromParent();
    return true;
  }

  // Otherwise load it from the constant pool, which is addressed from the
  // global pointer.
  const Constant *C =
      ConstantInt::get(Type::getInt64Ty(MF.getFunction().getContext()), V);
  unsigned CPI = MF.getConstantPool()->getConstantPoolIndex(C, Align(8));

  emitGprelPair(I, Alpha::LDQg, Dst, MachineOperand::CreateCPI(CPI, 0), MRI);

  I.eraseFromParent();
  return true;
}

// Converting between an integer and a floating value happens in a floating
// register, so the value has to be moved into or out of one first -- through
// memory, since no instruction moves between the banks.
bool AlphaInstructionSelector::selectIntFPConv(MachineInstr &I,
                                               MachineRegisterInfo &MRI) const {
  bool ToFP = I.getOpcode() == TargetOpcode::G_SITOFP;
  Register Dst = I.getOperand(0).getReg();
  Register Src = I.getOperand(1).getReg();

  if (ToFP) {
    // The integer is moved into a floating register and converted there; which
    // convert depends on the type wanted back.
    LLT DstTy = MRI.getType(Dst);
    unsigned Cvt = DstTy == LLT::scalar(32) ? Alpha::CVTQS : Alpha::CVTQT;

    Register Moved = MRI.createVirtualRegister(&Alpha::FPRCRegClass);
    emit(I, Alpha::MOVi2f, Moved).addUse(Src);

    emit(I, Cvt, Dst).addUse(Moved);
  } else {
    // A float in a register is already in T_floating form, so one convert
    // serves both widths; the result is an integer sitting in a floating
    // register, which then has to be moved out.
    Register Converted = MRI.createVirtualRegister(&Alpha::FPRCRegClass);
    emit(I, Alpha::CVTTQ, Converted).addUse(Src);

    emit(I, Alpha::MOVf2i, Dst).addUse(Converted);
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
    emit(I, CheapOpc, I.getOperand(0).getReg())
        .addUse(Alpha::F31)
        .addUse(Alpha::F31);
    I.eraseFromParent();
    return true;
  }

  Align Alignment(Ty.getSizeInBytes());
  unsigned CPI = MF.getConstantPool()->getConstantPoolIndex(CFP, Alignment);

  emitGprelPair(I, Ty == LLT::scalar(32) ? Alpha::LDSg : Alpha::LDTg,
                I.getOperand(0).getReg(), MachineOperand::CreateCPI(CPI, 0),
                MRI);

  I.eraseFromParent();
  return true;
}

// Extracting a byte, word or longword from a fixed byte position: `extbl $x,
// 1' is the low byte of `x >> 8'.  The SelectionDAG path spells this as three
// Pat<>s in AlphaInstrInfo.td, but their shift operand is a PatLeaf with a C++
// predicate and their immediate goes through an SDNodeXForm, neither of which
// GlobalISelEmitter can import, so the same three patterns are written out
// here.  Without this the extract is a shift and a mask -- two instructions,
// and for the longword mask a zapnot whose operand had to be materialized.
bool AlphaInstructionSelector::selectFieldExtract(
    MachineInstr &I, MachineRegisterInfo &MRI) const {
  Register Dst = I.getOperand(0).getReg();
  if (MRI.getType(Dst) != LLT::scalar(64))
    return false;

  auto Cst = getIConstantVRegValWithLookThrough(I.getOperand(2).getReg(), MRI);
  if (!Cst)
    return false;
  unsigned Opc;
  switch (Cst->Value.getZExtValue()) {
  case 0xFF:
    Opc = Alpha::EXTBLi;
    break;
  case 0xFFFF:
    Opc = Alpha::EXTWLi;
    break;
  case 0xFFFFFFFF:
    Opc = Alpha::EXTLLi;
    break;
  default:
    return false;
  }

  Register Src = I.getOperand(1).getReg();
  MachineInstr *Shift = MRI.getVRegDef(Src);
  if (!Shift || Shift->getOpcode() != TargetOpcode::G_LSHR ||
      !MRI.hasOneNonDBGUse(Src))
    return false;
  std::optional<int64_t> Amt =
      getIConstantVRegSExtVal(Shift->getOperand(2).getReg(), MRI);
  // A multiple of eight bits, and within the register: the instruction's
  // literal is a byte position, so anything else has no spelling here.
  if (!Amt || *Amt < 0 || *Amt > 56 || (*Amt & 7) != 0)
    return false;

  emit(I, Opc, Dst).addUse(Shift->getOperand(1).getReg()).addImm(*Amt / 8);
  I.eraseFromParent();
  Shift->eraseFromParent();
  return true;
}

// A literal the imported patterns cannot reach.  Three of Alpha's logical
// operations take the *complement* of their literal -- bic, ornot and eqv --
// and subtracting a small constant is an lda of its negation.  The
// SelectionDAG path spells all four as Pat<>s whose immediate goes through an
// SDNodeXForm (`notImm', and the negated displacement), which
// GlobalISelEmitter cannot import, so the same four are written out here.
// Without this each is two instructions: the constant materialised into a
// register, then the register-register form.
bool AlphaInstructionSelector::selectAluImm(MachineInstr &I,
                                            MachineRegisterInfo &MRI) const {
  Register Dst = I.getOperand(0).getReg();
  if (MRI.getType(Dst) != LLT::scalar(64))
    return false;
  Register Src = I.getOperand(1).getReg();
  std::optional<int64_t> C =
      getIConstantVRegSExtVal(I.getOperand(2).getReg(), MRI);
  if (!C)
    return false;

  if (I.getOpcode() == TargetOpcode::G_SUB) {
    // s4subq and s8subq take a literal too, and scale the first operand, so
    // the shift feeding them is free.  This has to be asked before the lda
    // below, which would take the subtraction and leave the shift standing.
    MachineInstr *Shl = getDefIgnoringCopies(Src, MRI);
    if (isUInt<8>(*C) && Shl && Shl->getOpcode() == TargetOpcode::G_SHL &&
        MRI.hasOneNonDBGUse(Src)) {
      if (auto K = getIConstantVRegSExtVal(Shl->getOperand(2).getReg(), MRI))
        if (*K == 2 || *K == 3) {
          emit(I, *K == 2 ? Alpha::S4SUBQi : Alpha::S8SUBQi, Dst)
              .addUse(Shl->getOperand(1).getReg())
              .addImm(*C);
          I.eraseFromParent();
          return true;
        }
    }
    // subq takes a literal, but neither path has a pattern for it: the
    // negated form is an lda, the same one instruction over a sixteen-bit
    // range where the literal field reaches only eight bits unsigned.
    if (!isInt<16>(-*C))
      return false;
    emit(I, Alpha::LDA, Dst).addImm(-*C).addUse(Src);
  } else {
    if (!isUInt<8>(~*C))
      return false;
    unsigned Opc = I.getOpcode() == TargetOpcode::G_AND  ? Alpha::BICi
                   : I.getOpcode() == TargetOpcode::G_OR ? Alpha::ORNOTi
                                                         : Alpha::EQVi;
    emit(I, Opc, Dst).addUse(Src).addImm(~*C);
  }

  I.eraseFromParent();
  return true;
}

// A 32-bit add, subtract or multiply whose result is used as a signed 64-bit
// value.  The legalizer widens the operation to a quadword and asks for the
// low longword back, which arrives here as G_SEXT(G_TRUNC(op)); selecting the
// three parts separately gives the quadword form and a separate `addl $r, $31'
// to sign-extend it.  Alpha's `l' forms do both in one instruction, which is
// what the SelectionDAG path gets from its own i32 patterns, so the whole
// shape is matched here.  The scaled forms are included because s4addl and its
// siblings are the same instruction with a shift built in, and the shift is
// still generic at this point.
bool AlphaInstructionSelector::selectNarrowArith(
    MachineInstr &I, MachineRegisterInfo &MRI) const {
  // The quadword result must be wanted only as the sign-extended longword;
  // anything else reading it needs the full 64-bit answer.
  MachineInstr *Trunc = getDefIgnoringCopies(I.getOperand(1).getReg(), MRI);
  if (!Trunc || Trunc->getOpcode() != TargetOpcode::G_TRUNC ||
      !MRI.hasOneNonDBGUse(Trunc->getOperand(0).getReg()))
    return false;
  Register WideReg = Trunc->getOperand(1).getReg();
  if (MRI.getType(WideReg) != LLT::scalar(64) || !MRI.hasOneNonDBGUse(WideReg))
    return false;
  MachineInstr *Op = getDefIgnoringCopies(WideReg, MRI);
  if (!Op)
    return false;

  bool IsAdd = Op->getOpcode() == TargetOpcode::G_ADD;
  bool IsSub = Op->getOpcode() == TargetOpcode::G_SUB;
  if (!IsAdd && !IsSub && Op->getOpcode() != TargetOpcode::G_MUL)
    return false;

  Register LHS = Op->getOperand(1).getReg();
  Register RHS = Op->getOperand(2).getReg();

  // s4addl/s8addl and their subtract counterparts scale the first operand by
  // four or eight.  Only an add or a subtract has them.
  unsigned Scale = 0;
  if (IsAdd || IsSub) {
    MachineInstr *Shl = getDefIgnoringCopies(LHS, MRI);
    if (Shl && Shl->getOpcode() == TargetOpcode::G_SHL &&
        MRI.hasOneNonDBGUse(LHS)) {
      if (auto K = getIConstantVRegSExtVal(Shl->getOperand(2).getReg(), MRI))
        if (*K == 2 || *K == 3) {
          Scale = *K;
          LHS = Shl->getOperand(1).getReg();
        }
    }
  }

  // A literal operand is folded into the instruction, which is where the
  // second form of each of these opcodes comes from.
  std::optional<int64_t> Lit = getIConstantVRegSExtVal(RHS, MRI);
  bool UseLit = Lit && isUInt<8>(*Lit);

  unsigned Opc;
  if (Scale == 2)
    Opc = IsAdd ? (UseLit ? Alpha::S4ADDLi : Alpha::S4ADDL)
                : (UseLit ? Alpha::S4SUBLi : Alpha::S4SUBL);
  else if (Scale == 3)
    Opc = IsAdd ? (UseLit ? Alpha::S8ADDLi : Alpha::S8ADDL)
                : (UseLit ? Alpha::S8SUBLi : Alpha::S8SUBL);
  else if (IsAdd)
    Opc = UseLit ? Alpha::ADDLi : Alpha::ADDL;
  else if (IsSub)
    Opc = UseLit ? Alpha::SUBLi : Alpha::SUBL;
  else
    Opc = UseLit ? Alpha::MULLi : Alpha::MULL;

  MachineInstrBuilder MIB = emit(I, Opc, I.getOperand(0).getReg()).addUse(LHS);
  if (UseLit)
    MIB.addImm(*Lit);
  else
    MIB.addUse(RHS);

  I.eraseFromParent();
  return true;
}

// Whether an instruction is `G_AND x, 1', the mask a boolean low-bit test
// leaves behind.
static bool isLowBitMask(const MachineInstr &MI,
                         const MachineRegisterInfo &MRI) {
  std::optional<int64_t> C =
      getIConstantVRegSExtVal(MI.getOperand(2).getReg(), MRI);
  return C && *C == 1;
}

// A conditional branch tests one register against zero, so a comparison that
// is itself against zero needs no compare instruction at all: the branch does
// it.  This is the GlobalISel counterpart of AlphaTargetLowering::LowerBR_CC,
// and recognizes the same forms it does, including the +/-1 spellings the
// middle end canonicalizes the "or equal" relations against zero into.
//
// Only a compare with a single use is folded.  Folding one with another user
// would leave the compare to be computed anyway and add a branch that repeats
// its work, which is a wash at best.
bool AlphaInstructionSelector::selectBrCond(MachineInstr &I,
                                            MachineRegisterInfo &MRI) const {
  Register Cond = I.getOperand(0).getReg();
  MachineBasicBlock *Dest = I.getOperand(1).getMBB();
  MachineInstr *Def = MRI.getVRegDef(Cond);
  if (!Def || !MRI.hasOneNonDBGUse(Cond))
    return false;

  unsigned Opc = 0;
  Register Test;

  if (Def->getOpcode() == TargetOpcode::G_ICMP) {
    auto Pred =
        static_cast<CmpInst::Predicate>(Def->getOperand(1).getPredicate());
    std::optional<int64_t> RHS =
        getIConstantVRegSExtVal(Def->getOperand(3).getReg(), MRI);
    if (!RHS)
      return false;
    if (*RHS == 0) {
      switch (Pred) {
      case CmpInst::ICMP_EQ:
        Opc = Alpha::BEQ;
        break;
      case CmpInst::ICMP_NE:
        Opc = Alpha::BNE;
        break;
      case CmpInst::ICMP_SLT:
        Opc = Alpha::BLT;
        break;
      case CmpInst::ICMP_SLE:
        Opc = Alpha::BLE;
        break;
      case CmpInst::ICMP_SGT:
        Opc = Alpha::BGT;
        break;
      case CmpInst::ICMP_SGE:
        Opc = Alpha::BGE;
        break;
      default:
        return false;
      }
    } else if (*RHS == 1) {
      if (Pred == CmpInst::ICMP_SGE) // x >= 1  <=>  x > 0
        Opc = Alpha::BGT;
      else if (Pred == CmpInst::ICMP_SLT) // x < 1   <=>  x <= 0
        Opc = Alpha::BLE;
      else
        return false;
    } else if (*RHS == -1) {
      if (Pred == CmpInst::ICMP_SGT) // x > -1  <=>  x >= 0
        Opc = Alpha::BGE;
      else if (Pred == CmpInst::ICMP_SLE) // x <= -1 <=>  x < 0
        Opc = Alpha::BLT;
      else
        return false;
    } else {
      return false;
    }
    Test = Def->getOperand(2).getReg();
  } else if (Def->getOpcode() == TargetOpcode::G_AND) {
    // The mask tested directly, with no compare against zero of its own.
    if (!isLowBitMask(*Def, MRI))
      return false;
    Opc = Alpha::BLBS;
    Test = Def->getOperand(1).getReg();
  } else {
    return false;
  }

  // A test of the low bit against zero folds the mask into the branch as well:
  // blbs branches when bit 0 is set and blbc when it is clear, which is what
  // the `AlphaBrNE (and GPRC:$Ra, 1)' pattern does on the SelectionDAG side.
  // This is a second level of folding, so it applies to whichever of the two
  // routes above produced an equality test.
  MachineInstr *Mask = MRI.getVRegDef(Test);
  if ((Opc == Alpha::BNE || Opc == Alpha::BEQ) && Mask &&
      Mask->getOpcode() == TargetOpcode::G_AND && MRI.hasOneNonDBGUse(Test) &&
      isLowBitMask(*Mask, MRI)) {
    Opc = Opc == Alpha::BNE ? Alpha::BLBS : Alpha::BLBC;
    Test = Mask->getOperand(1).getReg();
  } else {
    Mask = nullptr;
  }

  emit(I, Opc).addUse(Test).addMBB(Dest);
  I.eraseFromParent();
  Def->eraseFromParent();
  if (Mask)
    Mask->eraseFromParent();
  return true;
}

// Select one instruction, then give real register classes to everything
// emit() built for it.  A failed selection leaves the function to the fallback
// path, which discards what was built, so there is nothing to constrain.
bool AlphaInstructionSelector::select(MachineInstr &I) {
  ToConstrain.clear();
  if (!selectInstr(I))
    return false;
  for (MachineInstr *Built : ToConstrain)
    // A copy is not constrained here: both its ends are registers something
    // else has already named a class for, and COPY has no operand description
    // to constrain them against.
    if (!Built->isCopy())
      constrainSelectedInstRegOperands(*Built, TII, TRI, RBI);
  return true;
}

bool AlphaInstructionSelector::selectInstr(MachineInstr &I) const {
  MachineRegisterInfo &MRI = I.getParent()->getParent()->getRegInfo();

  if (!I.isPreISelOpcode()) {
    if (I.isCopy() || I.isPHI())
      return selectCopy(I, MRI, RBI);
    return true;
  }

  // Before selectImpl, which matches the register-register and and leaves the
  // mask to be materialized.
  if (I.getOpcode() == TargetOpcode::G_AND && selectFieldExtract(I, MRI))
    return true;

  // Before selectImpl as well: three of the logical operations take the
  // complement of their literal, and a subtraction of a small constant is an
  // lda of its negation.  selectImpl would take the register-register form
  // first and leave the constant to be materialised.
  if ((I.getOpcode() == TargetOpcode::G_AND ||
       I.getOpcode() == TargetOpcode::G_OR ||
       I.getOpcode() == TargetOpcode::G_XOR ||
       I.getOpcode() == TargetOpcode::G_SUB) &&
      selectAluImm(I, MRI))
    return true;

  // Before selectImpl too, which matches the bare `brcond GPRC:$Ra' pattern on
  // BNE and so would take the branch before the compare feeding it could be
  // folded in.
  if (I.getOpcode() == TargetOpcode::G_BRCOND && selectBrCond(I, MRI))
    return true;

  // A fence within a single thread orders nothing another processor can see:
  // it exists only to keep the compiler from moving accesses across it, which
  // MEMBARRIER says without asking for an instruction.  The imported pattern
  // does not look at the scope, so without this a single-thread fence becomes
  // a real mb bought for nothing.  The SelectionDAG path lowers it the same
  // way.
  if (I.getOpcode() == TargetOpcode::G_FENCE &&
      static_cast<SyncScope::ID>(I.getOperand(1).getImm()) ==
          SyncScope::SingleThread) {
    I.setDesc(TII.get(TargetOpcode::MEMBARRIER));
    while (I.getNumOperands())
      I.removeOperand(I.getNumOperands() - 1);
    return true;
  }

  // Zero lives in a register, so it needs no instruction at all -- but the
  // imported `lda $r, 0($31)' pattern would take it first, and every consumer
  // that could have read $31 directly (a store of zero, above all) would then
  // read the materialised copy instead.
  if (I.getOpcode() == TargetOpcode::G_CONSTANT &&
      I.getOperand(1).getCImm()->isZero() &&
      RBI.getRegBank(I.getOperand(0).getReg(), MRI, TRI)->getID() ==
          Alpha::GPRRegBankID) {
    Register Dst = I.getOperand(0).getReg();
    emit(I, TargetOpcode::COPY, Dst).addUse(Alpha::R31);
    RBI.constrainGenericRegister(Dst, Alpha::GPRCRegClass, MRI);
    I.eraseFromParent();
    return true;
  }

  if (selectImpl(I, *CoverageInfo))
    return true;

  switch (I.getOpcode()) {
  case TargetOpcode::G_IMPLICIT_DEF:
    // Nothing to compute: the register just has to be given a class.
    I.setDesc(TII.get(TargetOpcode::IMPLICIT_DEF));
    RBI.constrainGenericRegister(
        I.getOperand(0).getReg(),
        RBI.getRegBank(I.getOperand(0).getReg(), MRI, TRI)->getID() ==
                Alpha::FPRRegBankID
            ? Alpha::FPRCRegClass
            : Alpha::GPRCRegClass,
        MRI);
    return true;
  case TargetOpcode::G_FREEZE:
  case TargetOpcode::G_CONSTANT_FOLD_BARRIER:
    // These only stop a value being reasoned about twice; they move nothing.
    I.setDesc(TII.get(TargetOpcode::COPY));
    constrainSelectedInstRegOperands(I, TII, TRI, RBI);
    return true;
  case TargetOpcode::G_LOAD:
  case TargetOpcode::G_STORE:
  case TargetOpcode::G_SEXTLOAD:
  case TargetOpcode::G_ZEXTLOAD:
    return selectLoadStore(I, MRI);
  case TargetOpcode::G_ICMP:
    return selectICmp(I, MRI);
  case TargetOpcode::G_FCMP:
    return selectFCmp(I, MRI);
  case TargetOpcode::G_SITOFP:
  case TargetOpcode::G_FPTOSI:
    return selectIntFPConv(I, MRI);
  case TargetOpcode::G_CONSTANT:
    return selectConstant(I, MRI);
  case TargetOpcode::G_SELECT:
    return selectSelect(I, MRI);
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
    if (isAlphaGprelAddressable(*GV)) {
      emitGprelPair(I, Alpha::LDAg, I.getOperand(0).getReg(), I.getOperand(1),
                    MRI);
    } else {
      emit(I, Alpha::LDQl, I.getOperand(0).getReg()).add(I.getOperand(1));
    }
    I.eraseFromParent();
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
      emit(I, Alpha::ANDi, I.getOperand(0).getReg())
          .addUse(I.getOperand(1).getReg())
          .addImm(1);
      I.eraseFromParent();
      return true;
    }
    // Otherwise every value already occupies a whole register, so a widening
    // or narrowing that does not change the bits is a copy.
    I.setDesc(TII.get(TargetOpcode::COPY));
    return selectCopy(I, MRI, RBI);
  }
  case TargetOpcode::G_ZEXT: {
    // zapnot keeps the bytes its mask names and zeroes the rest, so it extends
    // any whole number of bytes; a boolean needs only its low bit.  A width
    // that is neither -- the source of an extension the legalizer built out of
    // a three-byte load, say -- has no byte mask, and a pair of shifts clears
    // everything above it instead.
    Register Dst = I.getOperand(0).getReg();
    Register Src = I.getOperand(1).getReg();
    unsigned Size = MRI.getType(Src).getSizeInBits();
    if (Size >= 64)
      return false;

    MachineInstrBuilder MIB;
    if (Size == 1) {
      MIB = emit(I, Alpha::ANDi, Dst).addUse(Src).addImm(1);
    } else if (Size == 8) {
      // The one byte mask that fits and's 8-bit literal field.  and is a
      // logic operation and zapnot a shift-class one, which is four issue
      // pipes rather than two on the 21264, either integer pipe rather than
      // E0 alone on the 21164, and one cycle rather than two on the 21064.
      MIB = emit(I, Alpha::ANDi, Dst).addUse(Src).addImm(0xFF);
    } else if (Size % 8 == 0) {
      MIB = emit(I, Alpha::ZAPNOTi, Dst)
                .addUse(Src)
                .addImm((1u << (Size / 8)) - 1);
    } else {
      unsigned Shift = 64 - Size;
      Register Tmp = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      emit(I, Alpha::SLLi, Tmp).addUse(Src).addImm(Shift);
      MIB = emit(I, Alpha::SRLi, Dst).addUse(Tmp).addImm(Shift);
    }
    I.eraseFromParent();
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
      // A longword operation whose result is sign-extended is one instruction:
      // the `l' forms compute the low 32 bits and sign-extend them in place.
      if (selectNarrowArith(I, MRI))
        return true;
      MIB = emit(I, Alpha::ADDL, Dst).addUse(Src).addUse(Alpha::R31);
    } else if (SrcTy.isScalar() &&
               (SrcTy.getSizeInBits() == 8 || SrcTy.getSizeInBits() == 16) &&
               I.getParent()
                   ->getParent()
                   ->getSubtarget<AlphaSubtarget>()
                   .hasBWX()) {
      // BWX has a one-instruction sign extension for a byte and for a word.
      MIB =
          emit(I, SrcTy.getSizeInBits() == 8 ? Alpha::SEXTB : Alpha::SEXTW, Dst)
              .addUse(Src);
    } else if (SrcTy.isScalar() && SrcTy.getSizeInBits() > 1 &&
               SrcTy.getSizeInBits() < 64) {
      // Any other width, a byte and a word among them: shift the field up to
      // the top of the register and back down again with an arithmetic shift.
      unsigned Shift = 64 - SrcTy.getSizeInBits();
      Register Tmp = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
      emit(I, Alpha::SLLi, Tmp).addUse(Src).addImm(Shift);
      MIB = emit(I, Alpha::SRAi, Dst).addUse(Tmp).addImm(Shift);
    } else if (SrcTy == LLT::scalar(1)) {
      // A boolean holds 0 or 1; negating it gives 0 or -1.
      MIB = emit(I, Alpha::SUBQ, Dst).addUse(Alpha::R31).addUse(Src);
    } else {
      return false;
    }
    I.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_BRCOND: {
    // A branch tests the whole register: the condition holds 0 or 1.
    emit(I, Alpha::BNE)
        .addUse(I.getOperand(0).getReg())
        .addMBB(I.getOperand(1).getMBB());
    I.eraseFromParent();
    return true;
  }
  case TargetOpcode::G_JUMP_TABLE:
  case TargetOpcode::G_BLOCK_ADDR:
  case TargetOpcode::G_CONSTANT_POOL:
    return selectGprelAddress(I, MRI);
  case TargetOpcode::G_BRJT:
    return selectBrJT(I, MRI);
  case TargetOpcode::G_BRINDIRECT: {
    // An indirect branch jumps to whatever address it is given; the only thing
    // that produces one is an indirectbr, whose target is a block address.
    MachineInstrBuilder MIB =
        BuildMI(*I.getParent(), I, I.getDebugLoc(), TII.get(Alpha::JMP))
            .addUse(I.getOperand(0).getReg());
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

// The address of something the linker resolves to a fixed distance from the
// global pointer: a jump table, a constant-pool entry, or a block address.
// All three are local to the object by construction, so none of them needs a
// GOT entry and all three are formed the same way a gp-relative global's
// address is -- an ldah/lda pair against $29, which is what LowerConstantPool,
// LowerJumpTable and LowerBlockAddress each build.
//
// The symbol operand is copied across whole rather than rebuilt, so that the
// index or block address and any offset on it survive: a constant-pool operand
// can carry one, and dropping it would name the start of the entry instead of
// the field being addressed.
bool AlphaInstructionSelector::selectGprelAddress(
    MachineInstr &I, MachineRegisterInfo &MRI) const {
  emitGprelPair(I, Alpha::LDAg, I.getOperand(0).getReg(), I.getOperand(1), MRI);
  I.eraseFromParent();
  return true;
}

// A jump table dispatch.  This is LowerBR_JT's sequence, instruction for
// instruction, and it has to be: the table it reads is written by
// AlphaAsmPrinter::emitJumpTableEntry, whose entries are 32-bit gp-relative
// offsets (R_ALPHA_GPREL32) because getJumpTableEncoding says EK_Custom32.
// A table of absolute addresses would be the simpler thing to jump through
// and would put a text relocation in every shared library that switched on
// anything, which is what GCC's PIC switch lowering avoids and why the entry
// kind is what it is.
//
// So: scale the index to a longword and add it to the table address, which is
// one s4addq; load the entry -- ldl, which sign-extends the longword it reads,
// so the offset arrives already widened -- add the global pointer back to
// recover the absolute address, and jump to it.
bool AlphaInstructionSelector::selectBrJT(MachineInstr &I,
                                          MachineRegisterInfo &MRI) const {
  MachineBasicBlock &MBB = *I.getParent();
  MachineFunction &MF = *MBB.getParent();
  const DebugLoc &DL = I.getDebugLoc();

  Register Table = I.getOperand(0).getReg();
  Register Index = I.getOperand(2).getReg();

  // The global pointer is live into the jump, and the entries are meaningless
  // without it.  G_JUMP_TABLE below sets this too, but a G_BRJT whose table
  // operand was rematerialised elsewhere must not depend on that having run.
  MF.getInfo<AlphaMachineFunctionInfo>()->setUsesGP();

  // The shift-and-add is written as one instruction here rather than left to a
  // combine, because there is no GlobalISel combiner on this target to do it.
  Register EntryAddr = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  MachineInstrBuilder Add =
      BuildMI(MBB, I, DL, TII.get(Alpha::S4ADDQ), EntryAddr)
          .addUse(Index)
          .addUse(Table);
  constrainSelectedInstRegOperands(*Add, TII, TRI, RBI);

  Register Offset = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  MachineInstrBuilder Ld = BuildMI(MBB, I, DL, TII.get(Alpha::LDL), Offset)
                               .addUse(EntryAddr)
                               .addImm(0);
  Ld.addMemOperand(MF.getMachineMemOperand(MachinePointerInfo::getJumpTable(MF),
                                           MachineMemOperand::MOLoad, 4,
                                           Align(4)));
  constrainSelectedInstRegOperands(*Ld, TII, TRI, RBI);

  Register Target = MRI.createVirtualRegister(&Alpha::GPRCRegClass);
  MachineInstrBuilder Abs = BuildMI(MBB, I, DL, TII.get(Alpha::ADDQ), Target)
                                .addUse(Alpha::R29)
                                .addUse(Offset);
  constrainSelectedInstRegOperands(*Abs, TII, TRI, RBI);

  MachineInstrBuilder Jmp =
      BuildMI(MBB, I, DL, TII.get(Alpha::JMP)).addUse(Target);
  constrainSelectedInstRegOperands(*Jmp, TII, TRI, RBI);

  I.eraseFromParent();
  return true;
}

namespace llvm {
InstructionSelector *
createAlphaInstructionSelector(const AlphaTargetMachine &TM,
                               const AlphaSubtarget &STI,
                               const AlphaRegisterBankInfo &RBI) {
  return new AlphaInstructionSelector(TM, STI, RBI);
}
} // end namespace llvm
