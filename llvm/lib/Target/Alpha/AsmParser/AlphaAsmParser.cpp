//===-- AlphaAsmParser.cpp - Parse Alpha assembly to MCInst --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/AlphaMCTargetDesc.h"
#include "TargetInfo/AlphaTargetInfo.h"
#include "llvm/ADT/StringExtras.h"
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
  if (getLexer().isNot(AsmToken::EndOfStatement))
    return Error(getLexer().getLoc(), "unexpected token");
  return false;
}

bool AlphaAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    Out.emitInstruction(Inst, getSTI());
    return false;
  case Match_MnemonicFail:
    return Error(IDLoc, "unrecognized instruction mnemonic");
  case Match_InvalidOperand:
    return Error(IDLoc, "invalid operand for instruction");
  }
  return Error(IDLoc, "failed to match instruction");
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeAlphaAsmParser() {
  RegisterMCAsmParser<AlphaAsmParser> X(getTheAlphaTarget());
}
