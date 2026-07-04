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
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
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

  void emitStartOfAsmFile(Module &M) override;
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

// Emit a .arch directive so an external assembler accepts the extension
// instructions we generate.  Choose the lowest architecture that covers the
// enabled features; a higher one harmlessly accepts a superset.  Only textual
// output needs this -- the integrated assembler encodes directly.
void AlphaAsmPrinter::emitStartOfAsmFile(Module &M) {
  if (!OutStreamer->hasRawTextSupport())
    return;
  const FeatureBitset &Features = TM.getMCSubtargetInfo().getFeatureBits();
  StringRef Arch = "ev4";
  if (Features[Alpha::FeatureFIX] || Features[Alpha::FeatureCIX])
    Arch = "ev6";
  else if (Features[Alpha::FeatureMVI])
    Arch = "pca56";
  else if (Features[Alpha::FeatureBWX])
    Arch = "ev56";
  OutStreamer->emitRawText(Twine("\t.arch ") + Arch);
}

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
  // An inline-asm memory operand is (base register, displacement), printed as
  // the usual `disp($base)`.
  const MachineOperand &Disp = MI->getOperand(OpNo + 1);
  O << (Disp.isImm() ? Disp.getImm() : 0) << '('
    << AlphaInstPrinter::getRegisterName(MI->getOperand(OpNo).getReg()) << ')';
  return false;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaAsmPrinter() {
  RegisterAsmPrinter<AlphaAsmPrinter> X(getTheAlphaTarget());
}
