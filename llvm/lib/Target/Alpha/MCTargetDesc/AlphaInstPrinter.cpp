//===- AlphaInstPrinter.cpp - Convert Alpha MCInst to asm syntax ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaInstPrinter.h"
#include "AlphaFixupKinds.h"
#include "AlphaMCTargetDesc.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

#include "AlphaGenAsmWriter.inc"

void AlphaInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &O) {
  // Splice a floating-point instruction's trap-mode and rounding-mode
  // qualifiers into its mnemonic (addt -> addt/su, addt/sud with dynamic
  // rounding, cvttq/c -> cvttq/svc).
  unsigned TrapClass = MII.get(MI->getOpcode()).TSFlags & Alpha::TrapClassMask;
  std::string Suffix;
  unsigned RM = Alpha::FPRoundNormal;
  if (Alpha::hasFPQual(MI->getFlags())) {
    // What was written, or what the bits say.  Printing anything else would
    // contradict the encoding this instruction already has.
    Suffix = Alpha::getFPTrapSpelling(Alpha::fpQualTrapBits(MI->getFlags()),
                                      /*IsIntOverflow=*/TrapClass == 3)
                 .str();
    RM = Alpha::fpQualRoundMode(MI->getFlags());
  } else {
    Suffix =
        Alpha::getFPTrapSuffix(TrapClass, STI.hasFeature(Alpha::FeatureIEEE),
                               STI.hasFeature(Alpha::FeatureIEEEInexact),
                               STI.hasFeature(Alpha::FeatureFPTrapU))
            .str();
    // Only a class that takes the ambient rounding mode gets it; float-to-
    // integer keeps whatever its own encoding says.
    RM = Alpha::fpRounds(TrapClass) ? getFPRoundMode(STI) : Alpha::FPRoundNormal;
  }
  if (Alpha::fpTakesWrittenRound(TrapClass))
    Suffix += Alpha::getFPRoundSuffix(RM).str();
  if (!Suffix.empty()) {
    std::string Buf;
    raw_string_ostream SS(Buf);
    printInstruction(MI, Address, SS);
    // The printed form is "\t<mnemonic> <operands>"; find the mnemonic (after
    // the leading whitespace) and the separator before the operands.
    size_t Start = Buf.find_first_not_of(" \t");
    size_t End = Buf.find_first_of(" \t", Start);
    size_t Slash = Buf.find('/', Start);
    if (Slash != std::string::npos && (End == std::string::npos || Slash < End))
      Buf.insert(Slash + 1, Suffix); // Merge before a rounding qualifier.
    else
      Buf.insert(End == std::string::npos ? Buf.size() : End, "/" + Suffix);
    O << Buf;
    printAnnotation(O, Annot);
    return;
  }
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void AlphaInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
}

// A branch displacement, in instructions from the one after the branch.  A
// disassembler that knows where the instruction is prints the address it
// reaches, as binutils does and as every other target here does; otherwise the
// reader is left doing the arithmetic against a number that means nothing on
// its own.
void AlphaInstPrinter::printBranchTarget(const MCInst *MI, uint64_t Address,
                                         int OpNum, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNum);
  if (!Op.isImm()) {
    printOperand(MI, OpNum, O);
    return;
  }
  if (!PrintBranchImmAsAddress) {
    O << Op.getImm();
    return;
  }
  uint64_t Target = Address + 4 + Op.getImm() * 4;
  Target &= 0xffffffffffffffffULL;
  O << formatHex(Target);
}

void AlphaInstPrinter::printOperand(const MCInst *MI, int OpNum,
                                    raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNum);
  if (MO.isReg())
    O << getRegisterName(MO.getReg());
  else if (MO.isImm())
    O << MO.getImm();
  else if (MO.isExpr())
    MAI.printExpr(O, *MO.getExpr());
  else
    llvm_unreachable("Invalid operand");
}

void AlphaInstPrinter::printParenReg(const MCInst *MI, int OpNum,
                                     raw_ostream &O) {
  O << '(' << getRegisterName(MI->getOperand(OpNum).getReg()) << ')';
}

void AlphaInstPrinter::printZeroDispReg(const MCInst *MI, int OpNum,
                                        raw_ostream &O) {
  // The unaligned ldq_u/stq_u take a memory operand; the assembler requires an
  // explicit zero displacement, so print `0($base)`.
  O << "0(" << getRegisterName(MI->getOperand(OpNum).getReg()) << ')';
}

void AlphaInstPrinter::printMemOperand(const MCInst *MI, int OpNum,
                                       raw_ostream &O) {
  // memri operands are (base register, displacement), printed as disp(base).
  // A relocation-specifier displacement prints its suffix after the base, so
  // `disp(base) !specifier` round-trips through the assembler.
  const MCOperand &Disp = MI->getOperand(OpNum + 1);
  const MCSpecifierExpr *Spec =
      Disp.isExpr() ? dyn_cast<MCSpecifierExpr>(Disp.getExpr()) : nullptr;
  if (Disp.isImm())
    O << Disp.getImm();
  else if (Spec)
    MAI.printExpr(O, *Spec->getSubExpr());
  else
    printOperand(MI, OpNum + 1, O);
  O << '(' << getRegisterName(MI->getOperand(OpNum).getReg()) << ')';
  if (Spec) {
    StringRef Name = Alpha::getSpecifierName(Spec->getSpecifier());
    if (!Name.empty())
      O << " !" << Name;
  }
}
