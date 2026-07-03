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
#include "MCTargetDesc/AlphaInstPrinter.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
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

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &O) override;

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
  case MachineOperand::MO_RegisterMask:
    // Register masks are not represented in the MCInst.
    return MCOperand();
  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());
  case MachineOperand::MO_MachineBasicBlock:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext));
  case MachineOperand::MO_GlobalAddress: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext);
    if (MO.getOffset())
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), OutContext), OutContext);
    return MCOperand::createExpr(Expr);
  }
  case MachineOperand::MO_ConstantPoolIndex:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetCPISymbol(MO.getIndex()), OutContext));
  case MachineOperand::MO_JumpTableIndex:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetJTISymbol(MO.getIndex()), OutContext));
  case MachineOperand::MO_ExternalSymbol:
    return MCOperand::createExpr(MCSymbolRefExpr::create(
        GetExternalSymbolSymbol(MO.getSymbolName()), OutContext));
  default:
    report_fatal_error("Alpha operand lowering is not yet implemented");
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

bool AlphaAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);

  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << AlphaInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return false;
  default:
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  }
}

bool AlphaAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                            unsigned OpNo,
                                            const char *ExtraCode,
                                            raw_ostream &O) {
  if (ExtraCode && ExtraCode[0])
    return true;
  // A memory operand is a base register; print it as an address `0($base)`.
  O << "0(" << AlphaInstPrinter::getRegisterName(MI->getOperand(OpNo).getReg())
    << ')';
  return false;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaAsmPrinter() {
  RegisterAsmPrinter<AlphaAsmPrinter> X(getTheAlphaTarget());
}
