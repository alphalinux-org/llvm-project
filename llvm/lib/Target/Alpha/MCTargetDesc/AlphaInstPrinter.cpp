//===- AlphaInstPrinter.cpp - Convert Alpha MCInst to asm syntax ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaInstPrinter.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
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
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

void AlphaInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  O << getRegisterName(Reg);
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
