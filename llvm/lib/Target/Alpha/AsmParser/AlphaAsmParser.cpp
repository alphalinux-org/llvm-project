//===-- AlphaAsmParser.cpp - Parse Alpha assembly to MCInst --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/AlphaFixupKinds.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"

using namespace llvm;

namespace {

class AlphaOperand : public MCParsedAsmOperand {
  enum KindTy { Token, Register, Immediate, Memory } Kind;
  SMLoc StartLoc, EndLoc;

  struct TokOp {
    const char *Data;
    unsigned Length;
  };
  struct RegOp {
    MCRegister Reg;
  };
  struct ImmOp {
    const MCExpr *Val;
  };
  struct MemOp {
    MCRegister Base;
    const MCExpr *Off;
  };
  union {
    TokOp Tok;
    RegOp RegK;
    ImmOp Imm;
    MemOp Mem;
  };

public:
  AlphaOperand(KindTy K) : Kind(K) {}

  bool isToken() const override { return Kind == Token; }
  bool isReg() const override { return Kind == Register; }
  bool isImm() const override { return Kind == Immediate; }
  bool isMem() const override { return Kind == Memory; }
  // The PALcode function code is a 26-bit field with nowhere to put anything
  // wider: without this, `call_pal 0x10000000' would assemble to call_pal 0.
  bool isPalFn() const {
    if (Kind != Immediate)
      return false;
    const auto *CE = dyn_cast<MCConstantExpr>(Imm.Val);
    return CE && isUInt<26>(CE->getValue());
  }
  // A register written in parentheses, `($reg)`, as jsr/jmp/ret/wh64 use: the
  // parser produces a memory operand (base with a zero displacement) that these
  // instructions consume as a plain base register.
  bool isParenReg() const { return Kind == Memory; }

  StringRef getToken() const {
    assert(Kind == Token);
    return StringRef(Tok.Data, Tok.Length);
  }
  MCRegister getReg() const override {
    assert(Kind == Register);
    return RegK.Reg;
  }
  const MCExpr *getImm() const {
    assert(Kind == Immediate);
    return Imm.Val;
  }
  MCRegister getMemBase() const {
    assert(Kind == Memory);
    return Mem.Base;
  }
  const MCExpr *getMemOff() const {
    assert(Kind == Memory);
    return Mem.Off;
  }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case Token:
      OS << "Token:" << getToken();
      break;
    case Register:
      OS << "Reg:" << RegK.Reg.id();
      break;
    case Immediate:
      OS << "Imm";
      break;
    case Memory:
      OS << "Mem";
      break;
    }
  }

  void addExpr(MCInst &Inst, const MCExpr *Expr) const {
    if (auto *CE = dyn_cast<MCConstantExpr>(Expr))
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createReg(getReg()));
  }
  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    addExpr(Inst, getImm());
  }
  void addMemOperands(MCInst &Inst, unsigned N) const {
    assert(N == 2);
    Inst.addOperand(MCOperand::createReg(Mem.Base));
    addExpr(Inst, Mem.Off);
  }
  void addParenRegOperands(MCInst &Inst, unsigned N) const {
    assert(N == 1);
    Inst.addOperand(MCOperand::createReg(Mem.Base));
  }

  // Wrap the displacement/immediate in a relocation-specifier expression from a
  // trailing `!literal` / `!gprelhigh` / ... suffix.
  void applySpecifier(unsigned Spec, MCContext &Ctx) {
    if (Kind == Memory)
      Mem.Off = MCSpecifierExpr::create(Mem.Off, Spec, Ctx);
    else if (Kind == Immediate)
      Imm.Val = MCSpecifierExpr::create(Imm.Val, Spec, Ctx);
  }

  static std::unique_ptr<AlphaOperand> createToken(StringRef Str, SMLoc S) {
    auto Op = std::make_unique<AlphaOperand>(Token);
    Op->Tok.Data = Str.data();
    Op->Tok.Length = Str.size();
    Op->StartLoc = S;
    Op->EndLoc = S;
    return Op;
  }
  static std::unique_ptr<AlphaOperand> createReg(MCRegister Reg, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<AlphaOperand>(Register);
    Op->RegK.Reg = Reg;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
  static std::unique_ptr<AlphaOperand> createImm(const MCExpr *Val, SMLoc S,
                                                 SMLoc E) {
    auto Op = std::make_unique<AlphaOperand>(Immediate);
    Op->Imm.Val = Val;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
  static std::unique_ptr<AlphaOperand>
  createMem(MCRegister Base, const MCExpr *Off, SMLoc S, SMLoc E) {
    auto Op = std::make_unique<AlphaOperand>(Memory);
    Op->Mem.Base = Base;
    Op->Mem.Off = Off;
    Op->StartLoc = S;
    Op->EndLoc = E;
    return Op;
  }
};

class AlphaAsmParser : public MCTargetAsmParser {
#define GET_ASSEMBLER_HEADER
#include "AlphaGenAsmMatcher.inc"

  bool matchRegister(StringRef Name, MCRegister &Reg);

public:
  enum AlphaMatchResultTy {
    Match_Dummy = FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "AlphaGenAsmMatcher.inc"
#undef GET_OPERAND_DIAGNOSTIC_TYPES
  };

  AlphaAsmParser(const MCSubtargetInfo &STI, MCAsmParser &P,
                 const MCInstrInfo &MII)
      : MCTargetAsmParser(STI, MII) {
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc, SMLoc &EndLoc) override;
  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;
  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;
  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  ParseStatus parseDirective(AsmToken DirectiveID) override;

  ParseStatus parseOperand(OperandVector &Operands);
  ParseStatus parseMemOperand(OperandVector &Operands);
};

} // end anonymous namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "AlphaGenAsmMatcher.inc"

bool AlphaAsmParser::matchRegister(StringRef Name, MCRegister &Reg) {
  Reg = MatchRegisterName(Name);
  return Reg == MCRegister();
}

ParseStatus AlphaAsmParser::parseDirective(AsmToken DirectiveID) {
  // `.arch <name>` selects the instruction set; enable the features it implies
  // so the extension instructions that follow assemble.
  if (DirectiveID.getIdentifier() == ".arch") {
    SMLoc Loc = getParser().getTok().getLoc();
    StringRef Arch;
    if (getParser().parseIdentifier(Arch))
      return Error(Loc, "expected architecture name after .arch");
    SmallVector<StringRef, 4> Feats;
    if (Arch == "ev56")
      Feats = {"bwx"};
    else if (Arch == "pca56")
      Feats = {"bwx", "mvi"};
    else if (Arch == "ev6" || Arch == "ev67" || Arch == "ev68")
      Feats = {"bwx", "cix", "fix", "mvi"};
    else if (Arch != "ev4" && Arch != "ev45" && Arch != "ev5")
      return Error(Loc, "unknown Alpha architecture '" + Arch + "'");
    MCSubtargetInfo &STI = copySTI();
    // .arch replaces the instruction set rather than adding to it, so `.arch
    // ev4' after `.arch ev6' narrows, as it does in GNU as.  Clearing first is
    // what makes that true; these four are the whole extension set.
    for (StringRef F : {"bwx", "cix", "fix", "mvi"})
      STI.ApplyFeatureFlag(("-" + F).str());
    for (StringRef F : Feats)
      STI.ApplyFeatureFlag(("+" + F).str());
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
    return ParseStatus::Success;
  }

  // `.set at`, `.set noat`, `.set macro`, `.set reorder`, and similar are
  // assembler mode pragmas that control features (the $28/$at temporary,
  // macro/reorder handling) we do not model; accept and ignore them.  A `.set`
  // with a symbol assignment is left to the generic parser.
  if (DirectiveID.getIdentifier() == ".set") {
    const AsmToken &Tok = getParser().getTok();
    if (Tok.is(AsmToken::Identifier)) {
      StringRef Opt = Tok.getIdentifier();
      if (Opt == "at" || Opt == "noat" || Opt == "macro" || Opt == "nomacro" ||
          Opt == "reorder" || Opt == "noreorder" || Opt == "move" ||
          Opt == "nomove" || Opt == "volatile" || Opt == "novolatile") {
        getParser().eatToEndOfStatement();
        return ParseStatus::Success;
      }
    }
  }
  return ParseStatus::NoMatch;
}

ParseStatus AlphaAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                             SMLoc &EndLoc) {
  const AsmToken &Tok = getParser().getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  if (Tok.isNot(AsmToken::Dollar))
    return ParseStatus::NoMatch;
  const AsmToken &RegTok = getLexer().peekTok();
  StringRef Name = RegTok.getString();
  if (matchRegister(("$" + Name).str(), Reg))
    return ParseStatus::NoMatch;
  getParser().Lex(); // $
  getParser().Lex(); // name
  return ParseStatus::Success;
}

bool AlphaAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  return !tryParseRegister(Reg, StartLoc, EndLoc).isSuccess();
}

ParseStatus AlphaAsmParser::parseOperand(OperandVector &Operands) {
  SMLoc S = getParser().getTok().getLoc();
  SMLoc E = getParser().getTok().getEndLoc();

  // Register: $<name>.
  if (getParser().getTok().is(AsmToken::Dollar)) {
    MCRegister Reg;
    if (tryParseRegister(Reg, S, E).isSuccess()) {
      Operands.push_back(AlphaOperand::createReg(Reg, S, E));
      return ParseStatus::Success;
    }
    return ParseStatus::Failure;
  }

  // An expression: either a bare immediate or the displacement of a memory
  // operand disp($base).  A memory operand may also start with '(' (disp 0).
  const MCExpr *Off;
  if (getParser().getTok().is(AsmToken::LParen)) {
    Off = MCConstantExpr::create(0, getContext());
  } else {
    if (getParser().parseExpression(Off))
      return ParseStatus::Failure;
    if (getParser().getTok().isNot(AsmToken::LParen)) {
      E = getParser().getTok().getLoc();
      Operands.push_back(AlphaOperand::createImm(Off, S, E));
      return ParseStatus::Success;
    }
  }

  // Memory operand: disp($base).
  getParser().Lex(); // (
  MCRegister Base;
  SMLoc RS, RE;
  if (!tryParseRegister(Base, RS, RE).isSuccess())
    return Error(getParser().getTok().getLoc(), "expected base register");
  if (getParser().getTok().isNot(AsmToken::RParen))
    return Error(getParser().getTok().getLoc(), "expected ')'");
  E = getParser().getTok().getEndLoc();
  getParser().Lex(); // )
  Operands.push_back(AlphaOperand::createMem(Base, Off, S, E));
  return ParseStatus::Success;
}

ParseStatus AlphaAsmParser::parseMemOperand(OperandVector &Operands) {
  return parseOperand(Operands);
}

bool AlphaAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                      StringRef Name, SMLoc NameLoc,
                                      OperandVector &Operands) {
  Operands.push_back(AlphaOperand::createToken(Name, NameLoc));

  if (getLexer().is(AsmToken::EndOfStatement))
    return false;

  if (!parseOperand(Operands).isSuccess())
    return true;
  while (getLexer().is(AsmToken::Comma)) {
    getParser().Lex(); // ,
    if (!parseOperand(Operands).isSuccess())
      return true;
  }

  // An optional relocation suffix `!name` (with an optional `!seq` number)
  // attaches a relocation specifier to the last operand.
  if (getLexer().is(AsmToken::Exclaim)) {
    getParser().Lex(); // !
    if (getLexer().isNot(AsmToken::Identifier))
      return Error(getLexer().getLoc(), "expected relocation name");
    StringRef R = getParser().getTok().getIdentifier();
    unsigned Spec = StringSwitch<unsigned>(R)
                        .Case("literal", Alpha::fixup_alpha_literal)
                        .Case("gprelhigh", Alpha::fixup_alpha_gprelhigh)
                        .Case("gprellow", Alpha::fixup_alpha_gprellow)
                        .Case("gpdisp", Alpha::fixup_alpha_gpdisp)
                        .Case("tprelhi", Alpha::fixup_alpha_tprelhi)
                        .Case("tprello", Alpha::fixup_alpha_tprello)
                        .Case("gottprel", Alpha::fixup_alpha_gottprel)
                        .Case("tlsgd", Alpha::fixup_alpha_tlsgd)
                        .Case("tlsldm", Alpha::fixup_alpha_tlsldm)
                        .Case("dtprelhi", Alpha::fixup_alpha_dtprelhi)
                        .Case("dtprello", Alpha::fixup_alpha_dtprello)
                        .Default(0);
    if (!Spec)
      return Error(getLexer().getLoc(), "unknown relocation name");
    getParser().Lex(); // name
    // Ignore the optional !seq sequence number used to pair relocations.
    if (getLexer().is(AsmToken::Exclaim)) {
      getParser().Lex(); // !
      getParser().Lex(); // number
    }
    static_cast<AlphaOperand &>(*Operands.back())
        .applySpecifier(Spec, getContext());
  }

  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getLexer().getLoc(), "unexpected token");
  return false;
}

// A parsed operand that is exactly the constant `V`.  The full spellings of
// ret and jmp below carry fields the bare encodings do not, so each is taken
// only where what it says is what the encoding holds.
static bool isConstImm(MCParsedAsmOperand &Op, int64_t V) {
  auto &AOp = static_cast<AlphaOperand &>(Op);
  if (!AOp.isImm())
    return false;
  const auto *CE = dyn_cast<MCConstantExpr>(AOp.getImm());
  return CE && CE->getValue() == V;
}

bool AlphaAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  StringRef Mnemonic = static_cast<AlphaOperand &>(*Operands[0]).getToken();

  // ldgp $Ra, disp($Rb): expand to ldah/lda with a GPDISP relocation (addend
  // 4) referencing the parsed base register.
  if (Mnemonic == "ldgp" && Operands.size() == 3) {
    // The destination is $Ra, not always $29: GNU as assembles
    // `ldgp $0, 0($27)' into a pair naming $0.  The displacement is carried by
    // the lda half.  Check the operand kinds before reading them, or a
    // register written where a memory operand belongs reads the wrong member
    // of the operand union.
    if (!Operands[1]->isReg())
      return Error(Operands[1]->getStartLoc(), "expected register operand");
    if (!Operands[2]->isMem())
      return Error(Operands[2]->getStartLoc(),
                   "expected memory operand of the form disp($reg)");
    MCRegister Dst = static_cast<AlphaOperand &>(*Operands[1]).getReg();
    MCRegister Base = static_cast<AlphaOperand &>(*Operands[2]).getMemBase();
    const MCExpr *Off = static_cast<AlphaOperand &>(*Operands[2]).getMemOff();
    const MCExpr *GpDisp =
        MCSpecifierExpr::create(MCConstantExpr::create(4, getContext()),
                                Alpha::fixup_alpha_gpdisp, getContext());
    MCInst Ldah;
    Ldah.setOpcode(Alpha::LDAHm);
    Ldah.addOperand(MCOperand::createReg(Dst));
    Ldah.addOperand(MCOperand::createReg(Base));
    Ldah.addOperand(MCOperand::createExpr(GpDisp));
    Ldah.setLoc(IDLoc);
    Out.emitInstruction(Ldah, getSTI());
    MCInst Lda;
    Lda.setOpcode(Alpha::LEA);
    Lda.addOperand(MCOperand::createReg(Dst));
    Lda.addOperand(MCOperand::createReg(Dst));
    if (const auto *CE = dyn_cast<MCConstantExpr>(Off))
      Lda.addOperand(MCOperand::createImm(CE->getValue()));
    else
      Lda.addOperand(MCOperand::createExpr(Off));
    Lda.setLoc(IDLoc);
    Out.emitInstruction(Lda, getSTI());
    return false;
  }

  // jsr $Ra, ($Rb): emit the bare jsr word.  The operand kinds are checked
  // too, so that a line whose operands are not what the form expects does not
  // read the wrong member of the operand union.
  if (Mnemonic == "jsr" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isMem()) {
    MCInst Inst;
    Inst.setOpcode(Alpha::JSRr);
    Inst.addOperand(MCOperand::createReg(
        static_cast<AlphaOperand &>(*Operands[1]).getReg()));
    Inst.addOperand(MCOperand::createReg(
        static_cast<AlphaOperand &>(*Operands[2]).getMemBase()));
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // ret $31, ($Rb), 1: the return written in full in hand assembly.  The
  // return target register $Rb is what matters -- it is not always $26 -- so
  // route it through the RETb form, which keeps the register it names.  RETb
  // still holds no hint of its own, so only the canonical 1 is taken here.
  if (Mnemonic == "ret" && Operands.size() == 4 && Operands[1]->isReg() &&
      Operands[2]->isMem() && isConstImm(*Operands[3], 1) &&
      static_cast<AlphaOperand &>(*Operands[1]).getReg() == Alpha::R31) {
    MCInst Inst;
    Inst.setOpcode(Alpha::RETb);
    Inst.addOperand(MCOperand::createReg(
        static_cast<AlphaOperand &>(*Operands[2]).getMemBase()));
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  // jmp $31, ($Rb), 0: an indirect jump through $Rb.  JMP holds neither the
  // link register nor the hint, so, as with ret above, only the spelling whose
  // fields the encoding can hold is taken here.
  if (Mnemonic == "jmp" && Operands.size() == 4 && Operands[1]->isReg() &&
      Operands[2]->isMem() && isConstImm(*Operands[3], 0) &&
      static_cast<AlphaOperand &>(*Operands[1]).getReg() == Alpha::R31) {
    MCInst Inst;
    Inst.setOpcode(Alpha::JMP);
    Inst.addOperand(MCOperand::createReg(
        static_cast<AlphaOperand &>(*Operands[2]).getMemBase()));
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  }

  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    // Whatever was written is what this instruction carries -- including
    // nothing, which is a qualifier too.  Recording it is what stops -mieee
    // from turning a hand-written `addt' into `addt/su': the encoder applies
    // the subtarget's policy only to an instruction that carries no qualifier
    // of its own, and everything the assembler sees carries one.
    if (MII.get(Inst.getOpcode()).TSFlags & Alpha::TrapClassMask)
      Inst.setFlags(Alpha::encodeFPQual(0, Alpha::FPRoundNormal));
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MnemonicFail:
    return Error(IDLoc, "unrecognized instruction mnemonic");
  case Match_InvalidOperand:
    return Error(IDLoc, "invalid operand for instruction");
  default:
    if (const char *Diag = getMatchKindDiag((AlphaMatchResultTy)Result)) {
      SMLoc Loc = IDLoc;
      if (ErrorInfo != ~0ULL && ErrorInfo < Operands.size())
        Loc = Operands[ErrorInfo]->getStartLoc();
      return Error(Loc, Diag);
    }
    break;
  }
  return Error(IDLoc, "failed to match instruction");
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaAsmParser() {
  RegisterMCAsmParser<AlphaAsmParser> X(getTheAlphaTarget());
}
