//===-- AlphaAsmPrinter.cpp - Alpha LLVM Assembly Printer -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to GAS-format Alpha assembly language.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "AlphaTargetMachine.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class AlphaAsmPrinter : public AsmPrinter {
public:
  explicit AlphaAsmPrinter(TargetMachine &TM,
                           std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer), ID) {}

  static char ID;

  StringRef getPassName() const override { return "Alpha Assembly Printer"; }

  void emitInstruction(const MachineInstr *MI) override;

private:
  MCOperand lowerOperand(const MachineOperand &MO) const;
};

} // end anonymous namespace

char AlphaAsmPrinter::ID = 0;

MCOperand AlphaAsmPrinter::lowerOperand(const MachineOperand &MO) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return MCOperand();
    return MCOperand::createReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());
  default:
    llvm_unreachable("unknown operand type");
  }
}

void AlphaAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst OutMI;
  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp = lowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
  EmitToStreamer(*OutStreamer, OutMI);
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaAsmPrinter() {
  RegisterAsmPrinter<AlphaAsmPrinter> X(getTheAlphaTarget());
}
