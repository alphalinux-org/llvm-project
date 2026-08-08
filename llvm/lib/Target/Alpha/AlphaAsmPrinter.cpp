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
  // ev6 covers CIX as well as FIX: GNU as's alpha_cpus table gives the name
  // AXP_OPCODE_CIX even though the 21264 part implements only the FIX half,
  // because .arch says what the assembler accepts rather than what the part
  // has.  Naming the part is -mcpu's job.
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
  case MachineOperand::MO_ConstantPoolIndex: {
    // The offset is part of the address: a constant pool entry can be indexed
    // into (a vector splat's element, an aggregate's field), and dropping it
    // names the start of the entry instead.
    const MCExpr *Expr =
        MCSymbolRefExpr::create(GetCPISymbol(MO.getIndex()), OutContext);
    if (MO.getOffset())
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), OutContext), OutContext);
    return MCOperand::createExpr(Expr);
  }
  case MachineOperand::MO_JumpTableIndex:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(GetJTISymbol(MO.getIndex()), OutContext));
  case MachineOperand::MO_ExternalSymbol:
    return MCOperand::createExpr(MCSymbolRefExpr::create(
        GetExternalSymbolSymbol(MO.getSymbolName()), OutContext));
  default:
    llvm_unreachable("unknown operand type");
  }
}

void AlphaAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // A bundle assembles to the instructions inside it, one after another; the
  // header carries nothing of its own.  Alpha uses one to hold the
  // read-modify-write a pre-BWX byte or misaligned store expands into.
  MachineBasicBlock::const_instr_iterator I = MI->getIterator();
  MachineBasicBlock::const_instr_iterator E = MI->getParent()->instr_end();
  do {
    if (I->isBundle())
      continue;
    MCInst OutMI;
    OutMI.setOpcode(I->getOpcode());
    for (const MachineOperand &MO : I->operands()) {
      MCOperand MCOp = lowerOperand(MO);
      if (MCOp.isValid())
        OutMI.addOperand(MCOp);
    }
    EmitToStreamer(*OutStreamer, OutMI);
  } while (++I != E && I->isInsideBundle());
}

bool AlphaAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                      const char *ExtraCode, raw_ostream &O) {
  const MachineOperand &MO = MI->getOperand(OpNo);

  if (ExtraCode && ExtraCode[0]) {
    // The 'r' modifier prints a register operand, substituting $31 (the zero
    // register) for a literal 0 -- the classic rJ inline-asm idiom.  A non-zero
    // literal prints as itself: gcc's print_operand case 'r' special-cases only
    // the zero and falls through to the ordinary operand printer otherwise, and
    // an operate instruction takes an 8-bit literal in the same position, so
    // `${0:r}' on an "I" operand is well-formed and must not be rejected.
    if (ExtraCode[0] == 'r' && ExtraCode[1] == 0) {
      if (MO.isImm()) {
        if (MO.getImm() == 0)
          O << "$31";
        else
          O << MO.getImm();
        return false;
      }
      if (MO.isReg()) {
        O << AlphaInstPrinter::getRegisterName(MO.getReg());
        return false;
      }
    }
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, O);
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    O << AlphaInstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_Immediate:
    O << MO.getImm();
    return false;
  case MachineOperand::MO_GlobalAddress:
    getSymbol(MO.getGlobal())->print(O, MAI);
    if (MO.getOffset())
      O << '+' << MO.getOffset();
    return false;
  case MachineOperand::MO_ExternalSymbol:
    GetExternalSymbolSymbol(MO.getSymbolName())->print(O, MAI);
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
