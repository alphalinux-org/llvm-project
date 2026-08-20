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
#include "AlphaMachineFunctionInfo.h"
#include "AlphaSubtarget.h"
#include "AlphaTargetMachine.h"
#include "MCTargetDesc/AlphaInstPrinter.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCSymbolELF.h"
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
  void emitFunctionBodyStart() override;
  void emitFunctionBodyEnd() override;
  void emitInstruction(const MachineInstr *MI) override;
  void emitJumpTableEntry(const MachineJumpTableInfo &MJTI,
                          const MachineBasicBlock *MBB,
                          unsigned UID) const override;

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &O) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &O) override;

private:
  MCOperand lowerOperand(const MachineOperand &MO) const;
  bool emitDirectCallText(const MachineInstr &MI);

  // The sequence number the next literal/lituse pair is written with.  GNU as
  // requires one on every !lituse_* and pairs it with the !literal carrying
  // the same number, so it has to be unique within the file rather than the
  // function.
  unsigned NextLituseSeq = 0;
  // The number the descriptor of a dynamic TLS sequence was written with,
  // which its call has to repeat.
  unsigned PendingTlsSeq = 0;
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

// Record the function's procedure kind in its st_other, as GNU as does from
// .ent/.prologue: a function that establishes its own global pointer advertises
// the standard gp load (STO_ALPHA_STD_GPLOAD) so a caller reaching it with a
// same-gp branch (R_ALPHA_BRSGP) may skip the gp-loading prologue; one that
// runs on the caller's gp needs no procedure value (STO_ALPHA_NOPV).
void AlphaAsmPrinter::emitFunctionBodyStart() {
  const unsigned STO_ALPHA_NOPV = 0x80, STO_ALPHA_STD_GPLOAD = 0x88;
  bool UsesGP = MF->getInfo<AlphaMachineFunctionInfo>()->usesGP();
  auto *Sym = static_cast<MCSymbolELF *>(CurrentFnSym);
  Sym->setOther((Sym->getOther() & ~STO_ALPHA_STD_GPLOAD) |
                (UsesGP ? STO_ALPHA_STD_GPLOAD : STO_ALPHA_NOPV));
  // For textual output an external assembler sets st_other from .ent/.prologue,
  // so emit those (.prologue 1 = standard gp load, 0 = runs on the caller's
  // gp).
  if (OutStreamer->hasRawTextSupport()) {
    OutStreamer->emitRawText(Twine("\t.ent ") + CurrentFnSym->getName());
    OutStreamer->emitRawText(Twine("\t.prologue ") + (UsesGP ? "1" : "0"));
    // -mieee-conformant sets PDSC_EXC_IEEE in the procedure descriptor, which
    // is what asks the loader for IEEE-conformant math-library routines.  gcc
    // emits this from alpha_start_function, per function and only in textual
    // output, and documents it as the option's whole effect.
    if (MF->getSubtarget<AlphaSubtarget>().hasIEEEConformant())
      OutStreamer->emitRawText(StringRef("\t.eflag 48"));
  }
}

void AlphaAsmPrinter::emitFunctionBodyEnd() {
  // Close the .ent opened above so an external assembler pairs them.
  if (OutStreamer->hasRawTextSupport())
    OutStreamer->emitRawText(Twine("\t.end ") + CurrentFnSym->getName());
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
  case MachineOperand::MO_BlockAddress: {
    const MCExpr *Expr = MCSymbolRefExpr::create(
        GetBlockAddressSymbol(MO.getBlockAddress()), OutContext);
    if (MO.getOffset())
      Expr = MCBinaryExpr::createAdd(
          Expr, MCConstantExpr::create(MO.getOffset(), OutContext), OutContext);
    return MCOperand::createExpr(Expr);
  }
  case MachineOperand::MO_ExternalSymbol:
    return MCOperand::createExpr(MCSymbolRefExpr::create(
        GetExternalSymbolSymbol(MO.getSymbolName()), OutContext));
  default:
    llvm_unreachable("unknown operand type");
  }
}

// Print a direct call or direct tail call, whose two relocations are written
// as a numbered pair.  The object path builds these fixups in the code emitter,
// where the numbers do not exist -- a fixup names its target directly.  The
// text path has only the assembler to say it to, and the assembler's way of
// saying "this call is the use of that literal" is !literal!N on the load and
// !lituse_jsr!N on the call.  Printing the load's relocation and not the
// call's, which a fixed AsmString cannot avoid (it has one operand and no
// counter), would lose the pairing --  and the pairing is the linker's only
// input for relaxing the pair into a branch.  gcc writes exactly this text.
bool AlphaAsmPrinter::emitDirectCallText(const MachineInstr &MI) {
  StringRef Lituse;
  bool IsTail = false, WantsHint = false;
  switch (MI.getOpcode()) {
  case Alpha::LDAtlsgd:
  case Alpha::LDAtlsldm: {
    // The descriptor address that opens a dynamic TLS sequence.  Its
    // relocation is the one the following call's !lituse_tls* is paired with,
    // so it needs the sequence number too, and the same one -- which is why it
    // is printed here rather than from its AsmString.
    if (!OutStreamer->hasRawTextSupport())
      return false;
    SmallString<128> Sym;
    raw_svector_ostream SymOS(Sym);
    PrintAsmOperand(&MI, 1, /*ExtraCode=*/nullptr, SymOS);
    PendingTlsSeq = ++NextLituseSeq;
    OutStreamer->emitRawText(
        Twine("\tlda ") +
        AlphaInstPrinter::getRegisterName(MI.getOperand(0).getReg()) + ", " +
        Sym + "($29)\t\t!" +
        (MI.getOpcode() == Alpha::LDAtlsgd ? "tlsgd" : "tlsldm") + "!" +
        Twine(PendingTlsSeq));
    return true;
  }
  case Alpha::JSRd:
    Lituse = "lituse_jsr";
    WantsHint = true;
    break;
  case Alpha::JSRdl:
    Lituse = "lituse_jsr";
    break;
  case Alpha::JSRtlsgd:
    Lituse = "lituse_tlsgd";
    break;
  case Alpha::JSRtlsldm:
    Lituse = "lituse_tlsldm";
    break;
  case Alpha::TCRETURNd:
    Lituse = "lituse_jsr";
    IsTail = true;
    WantsHint = true;
    break;
  case Alpha::TCRETURNdl:
    Lituse = "lituse_jsr";
    IsTail = true;
    break;
  default:
    return false;
  }
  if (!OutStreamer->hasRawTextSupport())
    return false;

  SmallString<128> Sym;
  raw_svector_ostream SymOS(Sym);
  PrintAsmOperand(&MI, 0, /*ExtraCode=*/nullptr, SymOS);

  // A dynamic TLS call takes the number its descriptor was written with: the
  // linker pairs the three by that one number.
  unsigned Seq = PendingTlsSeq ? PendingTlsSeq : ++NextLituseSeq;
  PendingTlsSeq = 0;
  OutStreamer->emitRawText(Twine("\tldq $27, ") + Sym + "($29)\t\t!literal!" +
                           Twine(Seq));
  // A callee the linker cannot reach with a branch takes a branch-prediction
  // hint as well, which is the callee named a second time in the jump's
  // displacement field.  A dso-local one deliberately does not: a hint would
  // pin the jsr and stop the relaxation the lituse exists to allow.
  OutStreamer->emitRawText(Twine(IsTail ? "\tjmp $31, ($27)" : "\tjsr $26, ($27)") +
                           (WantsHint ? Twine(", ") + Sym : Twine()) +
                           "\t\t!" + Lituse + "!" + Twine(Seq));
  if (!IsTail)
    OutStreamer->emitRawText(StringRef("\tldgp $29, 0($26)"));
  return true;
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
    // An instruction gated on a subtarget feature must not reach a subtarget
    // without it.  TableGen already builds the predicate table; this is the
    // only place every emitted instruction passes through, so it is where the
    // check belongs.  It compiles away in a release build.
    Alpha_MC::verifyInstructionPredicates(I->getOpcode(),
                                          getSubtargetInfo().getFeatureBits());
    if (emitDirectCallText(*I))
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

void AlphaAsmPrinter::emitJumpTableEntry(const MachineJumpTableInfo &MJTI,
                                         const MachineBasicBlock *MBB,
                                         unsigned UID) const {
  // Each entry is a GP-relative 32-bit offset, which the object path expresses
  // as a fixup built by LowerCustomJumpTableEntry.  In assembly the same thing
  // is spelled with the .gprel32 directive, as gcc and GNU as write it: the
  // `!'-suffix grammar attaches a relocation specifier to an instruction
  // operand, not to a data directive, so there is no `.long x !gprel32' to
  // print.  Without this the entry would come out as a bare `.long .LBB0_1',
  // which reassembles to R_ALPHA_REFLONG and makes the jump table hold
  // absolute addresses that the dispatch sequence then adds $29 to.
  assert(MJTI.getEntryKind() == MachineJumpTableInfo::EK_Custom32 &&
         "Alpha jump tables are EK_Custom32");
  if (!OutStreamer->hasRawTextSupport())
    return AsmPrinter::emitJumpTableEntry(MJTI, MBB, UID);

  SmallString<128> Str;
  raw_svector_ostream OS(Str);
  OS << "\t.gprel32\t";
  MBB->getSymbol()->print(OS, MAI);
  OutStreamer->emitRawText(OS.str());
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
  case MachineOperand::MO_BlockAddress:
    GetBlockAddressSymbol(MO.getBlockAddress())->print(O, MAI);
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
