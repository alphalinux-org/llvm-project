//===-- AlphaInstrInfo.cpp - Alpha Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaInstrInfo.h"
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineInstrBundle.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "AlphaGenInstrInfo.inc"

using namespace llvm;

AlphaInstrInfo::AlphaInstrInfo(const AlphaSubtarget &STI)
    : AlphaGenInstrInfo(STI, RI, Alpha::ADJCALLSTACKDOWN,
                        Alpha::ADJCALLSTACKUP),
      RI() {}

void AlphaInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 const DebugLoc &DL, Register DestReg,
                                 Register SrcReg, bool KillSrc,
                                 bool RenamableDest, bool RenamableSrc) const {
  if (Alpha::GPRCRegClass.contains(DestReg, SrcReg)) {
    // mov = bis $31, SrcReg, DestReg
    BuildMI(MBB, MI, DL, get(Alpha::BIS), DestReg)
        .addReg(Alpha::R31)
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  if (Alpha::FPRCRegClass.contains(DestReg, SrcReg)) {
    // fmov = cpys SrcReg, SrcReg, DestReg
    BuildMI(MBB, MI, DL, get(Alpha::CPYS), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrc))
        .addReg(SrcReg, getKillRegState(KillSrc));
    return;
  }
  llvm_unreachable("Alpha copyPhysReg: unsupported register class");
}

void llvm::addNarrowedMemOperands(MachineInstrBuilder MIB,
                                  const MachineInstr &MI,
                                  MachineMemOperand::Flags Half) {
  MachineFunction &MF = *MIB->getMF();
  for (MachineMemOperand *MMO : MI.memoperands()) {
    auto Flags = (MMO->getFlags() &
                  ~(MachineMemOperand::MOLoad | MachineMemOperand::MOStore)) |
                 Half;
    MIB.addMemOperand(MF.getMachineMemOperand(
        MMO->getPointerInfo(), Flags, MMO->getSize(), MMO->getBaseAlign(),
        MMO->getAAInfo(), MMO->getRanges(), MMO->getSyncScopeID(),
        MMO->getSuccessOrdering(), MMO->getFailureOrdering()));
  }
}

// Describe an access to the whole of a stack slot, so that what follows knows
// which object is touched and can tell one slot's traffic from another's.
static MachineMemOperand *getStackSlotMMO(MachineFunction &MF, int FrameIndex,
                                          MachineMemOperand::Flags Flags) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getMachineMemOperand(
      MachinePointerInfo::getFixedStack(MF, FrameIndex), Flags,
      MFI.getObjectSize(FrameIndex), MFI.getObjectAlign(FrameIndex));
}

void AlphaInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC, Register VReg,
    MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned Opc =
      Alpha::GPRCRegClass.hasSubClassEq(RC) ? Alpha::STQ : Alpha::STT;
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBBI, DL, get(Opc))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(
          getStackSlotMMO(MF, FrameIndex, MachineMemOperand::MOStore))
      .setMIFlag(Flags);
}

void AlphaInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          Register DestReg, int FrameIndex,
                                          const TargetRegisterClass *RC,
                                          Register VReg, unsigned SubReg,
                                          MachineInstr::MIFlag Flags) const {
  DebugLoc DL = MBBI != MBB.end() ? MBBI->getDebugLoc() : DebugLoc();
  unsigned Opc =
      Alpha::GPRCRegClass.hasSubClassEq(RC) ? Alpha::LDQ : Alpha::LDT;
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBBI, DL, get(Opc), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0)
      .addMemOperand(getStackSlotMMO(MF, FrameIndex, MachineMemOperand::MOLoad))
      .setMIFlag(Flags);
}

//===----------------------------------------------------------------------===//
// Branch analysis.
//===----------------------------------------------------------------------===//

// The conditional branches test a single register against zero.  Cond is
// { branch opcode, tested register }.
static bool isCondBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case Alpha::BEQ:
  case Alpha::BNE:
  case Alpha::BLT:
  case Alpha::BLE:
  case Alpha::BGT:
  case Alpha::BGE:
  case Alpha::BLBC:
  case Alpha::BLBS:
    return true;
  default:
    return false;
  }
}

// The opcode that branches on the opposite condition, or 0 if none.
static unsigned getReversedBranchOpcode(unsigned Opc) {
  switch (Opc) {
  case Alpha::BEQ:
    return Alpha::BNE;
  case Alpha::BNE:
    return Alpha::BEQ;
  case Alpha::BLT:
    return Alpha::BGE;
  case Alpha::BGE:
    return Alpha::BLT;
  case Alpha::BLE:
    return Alpha::BGT;
  case Alpha::BGT:
    return Alpha::BLE;
  case Alpha::BLBC:
    return Alpha::BLBS;
  case Alpha::BLBS:
    return Alpha::BLBC;
  default:
    return 0;
  }
}

bool AlphaInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false; // Falls through, no terminators to analyze.

  // An unconditional branch to the layout successor is redundant; dropping it
  // is what lets a caller that asked for it see through to the fallthrough.
  if (AllowModify) {
    while (I != MBB.end() && I->getOpcode() == Alpha::BR &&
           MBB.isLayoutSuccessor(I->getOperand(0).getMBB())) {
      I->eraseFromParent();
      I = MBB.getLastNonDebugInstr();
      if (I == MBB.end() || !isUnpredicatedTerminator(*I))
        return false;
    }
  }

  MachineInstr *LastInst = &*I;
  unsigned LastOpc = LastInst->getOpcode();

  bool HasSecond = I != MBB.begin() && isUnpredicatedTerminator(*std::prev(I));
  MachineInstr *SecondLast = HasSecond ? &*std::prev(I) : nullptr;

  if (!HasSecond) {
    if (LastOpc == Alpha::BR) {
      TBB = LastInst->getOperand(0).getMBB();
      return false;
    }
    if (isCondBranchOpcode(LastOpc)) {
      TBB = LastInst->getOperand(1).getMBB();
      Cond.push_back(MachineOperand::CreateImm(LastOpc));
      Cond.push_back(LastInst->getOperand(0));
      return false;
    }
    return true; // Indirect or otherwise unanalyzable.
  }

  if (isCondBranchOpcode(SecondLast->getOpcode()) && LastOpc == Alpha::BR) {
    // Only if there is no third one.  A block ending beq / bne / br is not a
    // two-way block: reporting it as one loses the first branch's edge, since
    // removeBranch erases at most two and insertBranch then puts back two.
    MachineBasicBlock::iterator Second = std::prev(I);
    if (Second != MBB.begin() && isUnpredicatedTerminator(*std::prev(Second)))
      return true;
    TBB = SecondLast->getOperand(1).getMBB();
    Cond.push_back(MachineOperand::CreateImm(SecondLast->getOpcode()));
    Cond.push_back(SecondLast->getOperand(0));
    FBB = LastInst->getOperand(0).getMBB();
    return false;
  }

  return true;
}

unsigned AlphaInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  unsigned Count = 0;
  while (I != MBB.end() && Count < 2) {
    unsigned Opc = I->getOpcode();
    if (Opc != Alpha::BR && !isCondBranchOpcode(Opc))
      break;
    I->eraseFromParent();
    if (BytesRemoved)
      *BytesRemoved += 4;
    ++Count;
    I = MBB.getLastNonDebugInstr();
  }
  return Count;
}

unsigned AlphaInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 2 || Cond.empty()) &&
         "Alpha branch conditions have one opcode and one register");

  // Unconditional branch.
  if (Cond.empty()) {
    assert(!FBB && "Unconditional branch has no false target");
    BuildMI(&MBB, DL, get(Alpha::BR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += 4;
    return 1;
  }

  // Conditional branch: Cond = { opcode, register }.
  unsigned Opc = Cond[0].getImm();
  BuildMI(&MBB, DL, get(Opc)).add(Cond[1]).addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += 4;
  if (!FBB)
    return 1;

  BuildMI(&MBB, DL, get(Alpha::BR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += 4;
  return 2;
}

bool AlphaInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 2 && "Invalid Alpha branch condition");
  unsigned Rev = getReversedBranchOpcode(Cond[0].getImm());
  if (!Rev)
    return true;
  Cond[0].setImm(Rev);
  return false;
}

unsigned AlphaInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isBundle()) {
    // The header itself assembles to nothing; the size is what the instructions
    // held inside it come to.
    unsigned Size = 0;
    for (auto I = std::next(MI.getIterator()), E = MI.getParent()->instr_end();
         I != E && I->isInsideBundle(); ++I)
      Size += getInstSizeInBytes(*I);
    return Size;
  }
  if (MI.isInlineAsm()) {
    const MachineFunction &MF = *MI.getParent()->getParent();
    const char *AsmStr = MI.getOperand(0).getSymbolName();
    return getInlineAsmLength(AsmStr, MF.getTarget().getMCAsmInfo());
  }
  return MI.getDesc().getSize();
}

bool AlphaInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  if (Opc == Alpha::RMW_USTORE) {
    // A misaligned store: read the one or two quadwords the field falls in,
    // splice the field into them and write them back.  Both reads come before
    // either write, so a field that spans two quadwords is written correctly
    // whichever way round they are; keeping the whole sequence together is what
    // stops another store to the same quadword from being lost.
    MachineBasicBlock &MBB = *MI.getParent();
    MachineBasicBlock::iterator End = std::next(MI.getIterator());
    DebugLoc DL = MI.getDebugLoc();
    Register HiAddr = MI.getOperand(0).getReg();
    Register Lo = MI.getOperand(1).getReg();
    Register Hi = MI.getOperand(2).getReg();
    Register Ins = MI.getOperand(3).getReg();
    Register Val = MI.getOperand(4).getReg();
    Register Addr = MI.getOperand(5).getReg();
    int64_t Bytes = MI.getOperand(6).getImm();

    unsigned InsL, InsH, MskL, MskH;
    switch (Bytes) {
    case 2:
      InsL = Alpha::INSWL;
      InsH = Alpha::INSWH;
      MskL = Alpha::MSKWL;
      MskH = Alpha::MSKWH;
      break;
    case 4:
      InsL = Alpha::INSLL;
      InsH = Alpha::INSLH;
      MskL = Alpha::MSKLL;
      MskH = Alpha::MSKLH;
      break;
    default:
      InsL = Alpha::INSQL;
      InsH = Alpha::INSQH;
      MskL = Alpha::MSKQL;
      MskH = Alpha::MSKQH;
      break;
    }

    MachineInstrBuilder First = BuildMI(MBB, MI, DL, get(Alpha::LDA), HiAddr)
                                    .addImm(Bytes - 1)
                                    .addReg(Addr);
    addNarrowedMemOperands(
        BuildMI(MBB, MI, DL, get(Alpha::LDQ_U), Lo).addReg(Addr), MI,
        MachineMemOperand::MOLoad);
    addNarrowedMemOperands(
        BuildMI(MBB, MI, DL, get(Alpha::LDQ_U), Hi).addReg(HiAddr), MI,
        MachineMemOperand::MOLoad);

    // The high quadword first: when the field does not cross a boundary the
    // two are the same quadword, and the high half is then empty, so writing it
    // first leaves the low half's write-back as the one that counts.
    BuildMI(MBB, MI, DL, get(MskH), Hi).addReg(Hi).addReg(Addr);
    BuildMI(MBB, MI, DL, get(InsH), Ins).addReg(Val).addReg(Addr);
    BuildMI(MBB, MI, DL, get(Alpha::BIS), Hi).addReg(Hi).addReg(Ins);
    addNarrowedMemOperands(
        BuildMI(MBB, MI, DL, get(Alpha::STQ_U)).addReg(Hi).addReg(HiAddr), MI,
        MachineMemOperand::MOStore);

    BuildMI(MBB, MI, DL, get(MskL), Lo).addReg(Lo).addReg(Addr);
    BuildMI(MBB, MI, DL, get(InsL), Ins).addReg(Val).addReg(Addr);
    BuildMI(MBB, MI, DL, get(Alpha::BIS), Lo).addReg(Lo).addReg(Ins);
    addNarrowedMemOperands(
        BuildMI(MBB, MI, DL, get(Alpha::STQ_U)).addReg(Lo).addReg(Addr), MI,
        MachineMemOperand::MOStore);

    MBB.erase(MI);
    finalizeBundle(MBB, First->getIterator(), End.getInstrIterator());
    return true;
  }

  if (Opc != Alpha::RMW_STOREI8 && Opc != Alpha::RMW_STOREI16)
    return false;

  // Update one field of the quadword holding it, in place.  Nothing may land
  // between the load and the store -- an update of another field of the same
  // quadword would be read before this one wrote it and then write the stale
  // copy back -- so the expansion goes into a bundle.  This pass runs before
  // the post-RA scheduler, not after it, so being a single instruction until
  // now is not by itself enough.
  bool IsByte = Opc == Alpha::RMW_STOREI8;
  MachineBasicBlock &MBB = *MI.getParent();
  MachineBasicBlock::iterator End = std::next(MI.getIterator());
  DebugLoc DL = MI.getDebugLoc();
  Register Tmp = MI.getOperand(0).getReg();
  Register Ins = MI.getOperand(1).getReg();
  Register Val = MI.getOperand(2).getReg();
  Register Addr = MI.getOperand(3).getReg();

  MachineInstrBuilder First =
      BuildMI(MBB, MI, DL, get(Alpha::LDQ_U), Tmp).addReg(Addr);
  addNarrowedMemOperands(First, MI, MachineMemOperand::MOLoad);
  BuildMI(MBB, MI, DL, get(IsByte ? Alpha::MSKBL : Alpha::MSKWL), Tmp)
      .addReg(Tmp)
      .addReg(Addr);
  BuildMI(MBB, MI, DL, get(IsByte ? Alpha::INSBL : Alpha::INSWL), Ins)
      .addReg(Val)
      .addReg(Addr);
  BuildMI(MBB, MI, DL, get(Alpha::BIS), Tmp).addReg(Tmp).addReg(Ins);
  addNarrowedMemOperands(
      BuildMI(MBB, MI, DL, get(Alpha::STQ_U)).addReg(Tmp).addReg(Addr), MI,
      MachineMemOperand::MOStore);

  MBB.erase(MI);
  finalizeBundle(MBB, First->getIterator(), End.getInstrIterator());
  return true;
}

bool AlphaInstrInfo::isSchedulingBoundary(const MachineInstr &MI,
                                          const MachineBasicBlock *MBB,
                                          const MachineFunction &MF) const {
  // Keep the entry ldgp pinned: its !gpdisp relocation resolves relative to the
  // ldah's own address, which only equals the incoming procedure value ($27)
  // when the ldgp is the first instruction, so nothing may move ahead of it.
  if (MI.getOpcode() == Alpha::LDGP)
    return true;
  return TargetInstrInfo::isSchedulingBoundary(MI, MBB, MF);
}

MachineBasicBlock *
AlphaInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  for (const MachineOperand &MO : MI.operands())
    if (MO.isMBB())
      return MO.getMBB();
  return nullptr;
}

bool AlphaInstrInfo::isBranchOffsetInRange(unsigned BranchOpc,
                                           int64_t BrOffset) const {
  // All branches carry a 21-bit signed displacement in 4-byte instruction
  // units, so the byte offset reaches +/- 4 MiB.
  return isInt<23>(BrOffset);
}

void AlphaInstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                          MachineBasicBlock &NewDestBB,
                                          MachineBasicBlock &RestoreBB,
                                          const DebugLoc &DL, int64_t BrOffset,
                                          RegScavenger *RS) const {
  // The destination is too far for a branch, so form its address gp-relatively
  // into the assembler scratch $28 (reserved, so nothing needs to be scavenged)
  // and jump through it: ldah !gprelhigh then lda !gprellow of the block
  // symbol.
  MachineFunction &MF = *MBB.getParent();
  BuildMI(MBB, MBB.end(), DL, get(Alpha::LDAHg), Alpha::R28)
      .addMBB(&NewDestBB)
      .addReg(Alpha::R29);
  BuildMI(MBB, MBB.end(), DL, get(Alpha::LDAg), Alpha::R28)
      .addMBB(&NewDestBB)
      .addReg(Alpha::R28);
  BuildMI(MBB, MBB.end(), DL, get(Alpha::JMP)).addReg(Alpha::R28);

  // Forming a gp-relative address needs the global pointer, and this runs after
  // the prologue that would establish it: asking for one here is too late.  The
  // guarantee that one is there comes from determineCalleeSaves, which requests
  // it for any function whose code approaches the branch range.
  assert(MF.getInfo<AlphaMachineFunctionInfo>()->usesGP() &&
         "relaxed branch has no global pointer to form its target with");
  (void)MF;
}

bool AlphaInstrInfo::isAssociativeAndCommutative(const MachineInstr &Inst,
                                                 bool Invert) const {
  // The inverse (subtract) forms are not modeled for reassociation.
  if (Invert)
    return false;
  switch (Inst.getOpcode()) {
  // 64-bit integer add, multiply and the bitwise operations are associative
  // and commutative.
  case Alpha::ADDQ:
  case Alpha::MULQ:
  case Alpha::AND:
  case Alpha::BIS:
  case Alpha::XOR:
    return true;
  // Floating-point add and multiply reassociate only when the fast-math flags
  // permit reordering and sign-of-zero changes.
  case Alpha::ADDS:
  case Alpha::ADDT:
  case Alpha::MULS:
  case Alpha::MULT:
    return Inst.getFlag(MachineInstr::MIFlag::FmReassoc) &&
           Inst.getFlag(MachineInstr::MIFlag::FmNsz);
  default:
    return false;
  }
}

//===----------------------------------------------------------------------===//
// Machine outlining.
//===----------------------------------------------------------------------===//

namespace {
// The single outlining construction this target uses: call the outlined
// function with a bsr, which saves the return address in $23 (a caller-saved
// temporary), and end it with a jump back through $23.
enum MachineOutlinerClass { MachineOutlinerDefault };
} // namespace

bool AlphaInstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return MF.getFunction().hasMinSize();
}

bool AlphaInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();
  // A linkonce_odr function may be deduplicated by the linker; don't outline
  // from it unless asked.  A function pinned to a section could expect all its
  // code to stay there.
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;
  if (F.hasSection())
    return false;
  return true;
}

std::optional<std::unique_ptr<outliner::OutlinedFunction>>
AlphaInstrInfo::getOutliningCandidateInfo(
    const MachineModuleInfo &MMI,
    std::vector<outliner::Candidate> &RepeatedSequenceLocs,
    unsigned MinRepeats) const {
  const TargetRegisterInfo &TRI =
      *RepeatedSequenceLocs[0].getMF()->getSubtarget().getRegisterInfo();

  // The outlined function is entered with a bsr that overwrites $23, so drop
  // any candidate where $23 is live across the call site.
  llvm::erase_if(RepeatedSequenceLocs, [&](outliner::Candidate &C) {
    return !C.isAvailableAcrossAndOutOfSeq(Alpha::R23, TRI);
  });
  if (RepeatedSequenceLocs.size() < MinRepeats)
    return std::nullopt;

  unsigned SequenceSize = 0;
  for (const MachineInstr &MI : RepeatedSequenceLocs[0])
    SequenceSize += getInstSizeInBytes(MI);

  // Each call is a single bsr; the outlined function adds one ret.
  for (outliner::Candidate &C : RepeatedSequenceLocs)
    C.setCallInfo(MachineOutlinerDefault, /*CallOverhead=*/4);

  return std::make_unique<outliner::OutlinedFunction>(
      RepeatedSequenceLocs, SequenceSize, /*FrameOverhead=*/4,
      MachineOutlinerDefault);
}

outliner::InstrType
AlphaInstrInfo::getOutliningTypeImpl(const MachineModuleInfo &MMI,
                                     MachineBasicBlock::iterator &MBBI,
                                     unsigned Flags) const {
  const MachineInstr &MI = *MBBI;
  const TargetRegisterInfo *TRI = MI.getMF()->getSubtarget().getRegisterInfo();

  // Positions and debug markers do not affect the outlined code.
  if (MI.isDebugInstr() || MI.isPosition())
    return outliner::InstrType::Invisible;

  // Outlining across CFI would split the unwind state; control-transfer and
  // frame instructions cannot be moved either.
  if (MI.isCFIInstruction() || MI.isCall() || MI.isReturn() || MI.isBranch() ||
      MI.isTerminator() || MI.isInlineAsm() ||
      MI.getFlag(MachineInstr::FrameSetup) ||
      MI.getFlag(MachineInstr::FrameDestroy))
    return outliner::InstrType::Illegal;

  // Anything that refers to a symbol, constant pool, jump table, block address,
  // frame slot, or carries a relocation specifier is position- or
  // gp-dependent, so it is unsafe to move into a separate function.
  for (const MachineOperand &MO : MI.operands()) {
    if (MO.isGlobal() || MO.isSymbol() || MO.isCPI() || MO.isJTI() ||
        MO.isBlockAddress() || MO.isMBB() || MO.isFI() ||
        MO.getTargetFlags() != 0)
      return outliner::InstrType::Illegal;
  }

  // The global pointer, stack pointer, frame pointer, procedure value, return
  // address, assembler scratch and the $23 bsr link either differ in the
  // outlined function or are clobbered by the call sequence.
  for (MCPhysReg Reg : {Alpha::R29, Alpha::R30, Alpha::R15, Alpha::R27,
                        Alpha::R28, Alpha::R26, Alpha::R23}) {
    if (MI.readsRegister(Reg, TRI) || MI.modifiesRegister(Reg, TRI))
      return outliner::InstrType::Illegal;
  }

  return outliner::InstrType::Legal;
}

void AlphaInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {
  // The bsr left the return address in $23; jump back through it.
  MBB.addLiveIn(Alpha::R23);
  MBB.insert(MBB.end(),
             BuildMI(MF, DebugLoc(), get(Alpha::JMP)).addReg(Alpha::R23));
}

MachineBasicBlock::iterator AlphaInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {
  It = MBB.insert(It, BuildMI(MF, DebugLoc(), get(Alpha::BSR))
                          .addGlobalAddress(M.getNamedValue(MF.getName())));
  return It;
}
