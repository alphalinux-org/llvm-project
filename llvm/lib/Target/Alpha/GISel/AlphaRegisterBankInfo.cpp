//===-- AlphaRegisterBankInfo.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the targeting of the RegisterBankInfo class for Alpha.
//
//===----------------------------------------------------------------------===//

#include "AlphaRegisterBankInfo.h"
#include "AlphaSubtarget.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_TARGET_REGBANK_IMPL
#include "AlphaGenRegisterBank.inc"

using namespace llvm;

namespace llvm {
namespace Alpha {

enum PartialMappingIdx {
  PMI_GPR64,
  PMI_FPR64,
  PMI_Min = PMI_GPR64,
};

const RegisterBankInfo::PartialMapping PartMappings[]{
    {0, 64, GPRRegBank},
    {0, 64, FPRRegBank},
};

enum ValueMappingIdx {
  InvalidIdx = 0,
  GPR3OpsIdx = 1,
  FPR3OpsIdx = 4,
};

const RegisterBankInfo::ValueMapping ValueMappings[] = {
    // invalid
    {nullptr, 0},
    // three operands in integer registers
    {&PartMappings[PMI_GPR64 - PMI_Min], 1},
    {&PartMappings[PMI_GPR64 - PMI_Min], 1},
    {&PartMappings[PMI_GPR64 - PMI_Min], 1},
    // three operands in floating-point registers
    {&PartMappings[PMI_FPR64 - PMI_Min], 1},
    {&PartMappings[PMI_FPR64 - PMI_Min], 1},
    {&PartMappings[PMI_FPR64 - PMI_Min], 1},
};

} // end namespace Alpha
} // end namespace llvm

// There is no way to tell an integer from a floating-point value by its type
// alone, so a load or a store is placed in the bank the value it moves comes
// from or goes to.
static bool isFPOpcode(unsigned Opc) {
  switch (Opc) {
  case TargetOpcode::G_FADD:
  case TargetOpcode::G_FSUB:
  case TargetOpcode::G_FMUL:
  case TargetOpcode::G_FDIV:
  case TargetOpcode::G_FNEG:
  case TargetOpcode::G_FABS:
  case TargetOpcode::G_FCONSTANT:
  case TargetOpcode::G_FPEXT:
  case TargetOpcode::G_FPTRUNC:
  case TargetOpcode::G_FSQRT:
  case TargetOpcode::G_FMA:
  case TargetOpcode::G_FREM:
  case TargetOpcode::G_FCOPYSIGN:
  case TargetOpcode::G_FMINNUM:
  case TargetOpcode::G_FMAXNUM:
    return true;
  // G_FCMP, G_SITOFP, G_FPTOSI, G_UITOFP, G_FPTOUI and G_SELECT are absent on
  // purpose.  This asks whether every register an opcode touches is floating,
  // which is what makes it usable for both the value a load defines and the
  // value a store uses.  Those six have one end in each bank -- a compare
  // answers in an integer register, a conversion has an integer on one side --
  // and G_SELECT follows the type of what it chooses between.  getInstrMapping
  // names their operands one at a time instead.
  default:
    return false;
  }
}

static bool isFPRegister(Register Reg, const MachineRegisterInfo &MRI) {
  if (Reg.isPhysical())
    return Alpha::FPRCRegClass.contains(Reg);
  if (const TargetRegisterClass *RC = MRI.getRegClassOrNull(Reg))
    return Alpha::FPRCRegClass.hasSubClassEq(RC);
  return false;
}

static bool definedByFP(Register Reg, const MachineRegisterInfo &MRI,
                        SmallPtrSetImpl<const MachineInstr *> *Seen = nullptr) {
  MachineInstr *Def = MRI.getVRegDef(Reg);
  if (!Def)
    return false;
  if (isFPOpcode(Def->getOpcode()))
    return true;
  // An argument arrives as a copy out of the register it was passed in.
  if (Def->isCopy() && Def->getOperand(1).getReg().isPhysical())
    return isFPRegister(Def->getOperand(1).getReg(), MRI);
  // A phi is floating when any value reaching it is, which is the same
  // question getInstrMapping asks of the phi itself.  Without this a value
  // that only ever comes out of a phi looks integer to the store that consumes
  // it, and the two banks then disagree -- the store reads it back through the
  // stack instead of storing the floating register it is already in.  The
  // walk is guarded because a loop's phi reaches itself.
  if (Def->isPHI()) {
    SmallPtrSet<const MachineInstr *, 8> Local;
    if (!Seen)
      Seen = &Local;
    if (!Seen->insert(Def).second)
      return false;
    for (unsigned I = 1, E = Def->getNumOperands(); I < E; I += 2)
      if (Def->getOperand(I).isReg() &&
          definedByFP(Def->getOperand(I).getReg(), MRI, Seen))
        return true;
  }
  return false;
}

static bool usedByFP(Register Reg, const MachineRegisterInfo &MRI) {
  for (const MachineInstr &Use : MRI.use_nodbg_instructions(Reg)) {
    if (isFPOpcode(Use.getOpcode()))
      return true;
    // A returned value is copied into the register it goes back in.
    if (Use.isCopy() && Use.getOperand(0).getReg().isPhysical() &&
        isFPRegister(Use.getOperand(0).getReg(), MRI))
      return true;
  }
  return false;
}

AlphaRegisterBankInfo::AlphaRegisterBankInfo() : AlphaGenRegisterBankInfo() {}

const RegisterBankInfo::InstructionMapping &
AlphaRegisterBankInfo::getInstrMapping(const MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  if (!isPreISelGenericOpcode(Opc)) {
    const InstructionMapping &Mapping = getInstrMappingImpl(MI);
    if (Mapping.isValid())
      return Mapping;
  }

  using namespace TargetOpcode;

  const MachineFunction &MF = *MI.getParent()->getParent();
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  unsigned NumOperands = MI.getNumOperands();

  const ValueMapping *OperandsMapping =
      &Alpha::ValueMappings[Alpha::GPR3OpsIdx];

  switch (Opc) {
  case G_ADD:
  case G_SUB:
  case G_MUL:
  case G_AND:
  case G_OR:
  case G_XOR:
  case G_SHL:
  case G_LSHR:
  case G_ASHR:
  case G_PTR_ADD:
    break;
  case G_LOAD:
  case G_STORE: {
    Register ValReg = MI.getOperand(0).getReg();
    bool IsFP =
        Opc == G_LOAD ? usedByFP(ValReg, MRI) : definedByFP(ValReg, MRI);
    OperandsMapping = getOperandsMapping(
        {&Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx],
         &Alpha::ValueMappings[Alpha::GPR3OpsIdx]});
    break;
  }
  case G_PHI: {
    // Every incoming value shares the bank of the result, which is what a
    // copy-like instruction's single mapping says; it must not be repeated per
    // operand, and the count below has to stay at one to match it.
    //
    // Since they all share it, the decision has to account for both ends: a phi
    // merging a value that came out of a floating operation belongs in the
    // floating bank even if nothing downstream says so, or the two would
    // disagree and there would be no way to copy between them.
    bool IsFP = usedByFP(MI.getOperand(0).getReg(), MRI);
    for (unsigned I = 1, E = NumOperands; I < E && !IsFP; I += 2)
      IsFP = definedByFP(MI.getOperand(I).getReg(), MRI);
    OperandsMapping =
        &Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx];
    break;
  }
  case G_BR:
    OperandsMapping = getOperandsMapping({nullptr});
    break;
  case G_BRCOND:
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::GPR3OpsIdx], nullptr});
    break;
  case G_ICMP:
    // The predicate operand carries no register.
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::GPR3OpsIdx], nullptr,
                            &Alpha::ValueMappings[Alpha::GPR3OpsIdx],
                            &Alpha::ValueMappings[Alpha::GPR3OpsIdx]});
    break;
  case G_FADD:
  case G_FSUB:
  case G_FMUL:
  case G_FDIV:
    OperandsMapping = &Alpha::ValueMappings[Alpha::FPR3OpsIdx];
    break;
  case G_FCMP:
    // The answer is an integer, the values compared are not, and the predicate
    // carries no register.
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::GPR3OpsIdx], nullptr,
                            &Alpha::ValueMappings[Alpha::FPR3OpsIdx],
                            &Alpha::ValueMappings[Alpha::FPR3OpsIdx]});
    break;
  case G_CONSTANT:
  case G_FRAME_INDEX:
  case G_GLOBAL_VALUE:
    // What these produce comes from an immediate, a frame index or a symbol,
    // none of which is a register and none of which may be given a mapping.
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::GPR3OpsIdx], nullptr});
    break;
  case G_INTTOPTR:
  case G_PTRTOINT:
  case G_ZEXT:
  case G_SEXT:
  case G_ANYEXT:
  case G_TRUNC:
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::GPR3OpsIdx],
                            &Alpha::ValueMappings[Alpha::GPR3OpsIdx]});
    break;
  case G_SELECT: {
    // The condition is an integer whichever bank the rest is in; the two values
    // and the result share a bank, decided from both ends as a phi's is, so
    // that choosing between two floating values is an fcmovne rather than a
    // round trip through memory.
    bool IsFP = usedByFP(MI.getOperand(0).getReg(), MRI) ||
                definedByFP(MI.getOperand(2).getReg(), MRI) ||
                definedByFP(MI.getOperand(3).getReg(), MRI);
    const ValueMapping *Val =
        &Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx];
    OperandsMapping = getOperandsMapping(
        {Val, &Alpha::ValueMappings[Alpha::GPR3OpsIdx], Val, Val});
    break;
  }
  case G_FPEXT:
  case G_FPTRUNC:
    OperandsMapping = &Alpha::ValueMappings[Alpha::FPR3OpsIdx];
    break;
  case G_FCONSTANT:
    OperandsMapping =
        getOperandsMapping({&Alpha::ValueMappings[Alpha::FPR3OpsIdx], nullptr});
    break;
  // All-integer operations, whose operands are all registers.
  case G_SMIN:
  case G_SMAX:
  case G_UMIN:
  case G_UMAX:
  case G_ABS:
  case G_BSWAP:
  case G_BITREVERSE:
  case G_CTLZ:
  case G_CTTZ:
  case G_CTPOP:
  case G_CTLZ_ZERO_POISON:
  case G_CTTZ_ZERO_POISON:
  case G_UMULH:
  case G_UADDO:
  case G_USUBO:
  case G_UADDE:
  case G_USUBE:
  case G_SADDO:
  case G_SSUBO:
  case G_SADDE:
  case G_SSUBE:
  case G_MERGE_VALUES:
  case G_UNMERGE_VALUES:
    break;
  // All-floating operations.
  case G_FNEG:
  case G_FABS:
  case G_FSQRT:
  case G_FCOPYSIGN:
    OperandsMapping = &Alpha::ValueMappings[Alpha::FPR3OpsIdx];
    break;
  // A bitcast is the one operation whose two ends may sit in different banks,
  // which is the whole reason it exists here.
  case G_BITCAST: {
    bool DstFP = usedByFP(MI.getOperand(0).getReg(), MRI);
    bool SrcFP = definedByFP(MI.getOperand(1).getReg(), MRI);
    OperandsMapping = getOperandsMapping(
        {&Alpha::ValueMappings[DstFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx],
         &Alpha::ValueMappings[SrcFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx]});
    break;
  }
  // A value with no computation behind it, and the barriers that pin one down,
  // belong wherever they are used -- the same question a phi asks.
  case G_IMPLICIT_DEF:
  case G_FREEZE:
  case G_CONSTANT_FOLD_BARRIER: {
    bool IsFP = usedByFP(MI.getOperand(0).getReg(), MRI);
    if (!IsFP && NumOperands > 1 && MI.getOperand(1).isReg())
      IsFP = definedByFP(MI.getOperand(1).getReg(), MRI);
    OperandsMapping =
        &Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx];
    if (NumOperands > 1)
      OperandsMapping = getOperandsMapping(
          {&Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx : Alpha::GPR3OpsIdx],
           &Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx
                                      : Alpha::GPR3OpsIdx]});
    else
      OperandsMapping =
          getOperandsMapping({&Alpha::ValueMappings[IsFP ? Alpha::FPR3OpsIdx
                                                         : Alpha::GPR3OpsIdx]});
    break;
  }
  // The ordering carries no register.
  case G_FENCE:
    OperandsMapping = getOperandsMapping({nullptr, nullptr});
    break;
  default:
    return getInvalidInstructionMapping();
  }

  // A copy-like instruction (a copy or a phi) carries one mapping covering all
  // of its operands rather than one per operand.
  if (MI.isCopy() || MI.isPHI())
    NumOperands = 1;

  return getInstructionMapping(DefaultMappingID, /*Cost=*/1, OperandsMapping,
                               NumOperands);
}
