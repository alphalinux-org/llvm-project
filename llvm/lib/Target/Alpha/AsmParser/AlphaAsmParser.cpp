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
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/ELF.h"
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
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/MathExtras.h"

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
  // The symbol named by the most recent `.ent`, whose st_other bits `.prologue`
  // sets.
  MCSymbol *CurEntSym = nullptr;

  // A floating-point qualifier written on the mnemonic, as MCInst flags, and
  // the mnemonic without it.  Set while parsing and consumed when matching:
  // most qualified operates are spelled `addt/su' with no def of their own, so
  // the match is retried against the base name with the qualifier recorded.
  unsigned PendingFPQual = 0;
  bool PendingFPQualIsV = false;
  StringRef PendingFPQualBase;

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
    // On Alpha `.word` is a 16-bit datum, matching GNU as.
    P.addAliasForDirective(".word", ".2byte");
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

  // Add a 32-bit signed value to Base with ldah/lda, writing the result to Rc;
  // returns the register now holding it (Rc, or Base if nothing was emitted).
  MCRegister emitConst32(MCRegister Rc, int32_t V32, MCRegister Base, SMLoc L,
                         MCStreamer &Out);
  // Materialize the 64-bit constant V into Rc entirely in code, matching the
  // isel constant builder (small values are one or two instructions; a wide
  // value builds its high half, shifts it up, then adds the low half).
  void emitLoadImm(MCRegister Rc, int64_t V, SMLoc L, MCStreamer &Out);
};

} // end anonymous namespace

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "AlphaGenAsmMatcher.inc"

bool AlphaAsmParser::matchRegister(StringRef Name, MCRegister &Reg) {
  Reg = MatchRegisterName(Name);
  if (Reg)
    return false;
  // The ABI register aliases (used throughout hand-written kernel assembly) are
  // not produced by the generated matcher, so map them here.
  Reg = StringSwitch<MCRegister>(Name)
            .Case("$v0", Alpha::R0)
            .Case("$t0", Alpha::R1)
            .Case("$t1", Alpha::R2)
            .Case("$t2", Alpha::R3)
            .Case("$t3", Alpha::R4)
            .Case("$t4", Alpha::R5)
            .Case("$t5", Alpha::R6)
            .Case("$t6", Alpha::R7)
            .Case("$t7", Alpha::R8)
            .Case("$s0", Alpha::R9)
            .Case("$s1", Alpha::R10)
            .Case("$s2", Alpha::R11)
            .Case("$s3", Alpha::R12)
            .Case("$s4", Alpha::R13)
            .Case("$s5", Alpha::R14)
            .Case("$fp", Alpha::R15)
            .Case("$s6", Alpha::R15)
            .Case("$a0", Alpha::R16)
            .Case("$a1", Alpha::R17)
            .Case("$a2", Alpha::R18)
            .Case("$a3", Alpha::R19)
            .Case("$a4", Alpha::R20)
            .Case("$a5", Alpha::R21)
            .Case("$t8", Alpha::R22)
            .Case("$t9", Alpha::R23)
            .Case("$t10", Alpha::R24)
            .Case("$t11", Alpha::R25)
            .Case("$ra", Alpha::R26)
            .Case("$pv", Alpha::R27)
            .Case("$t12", Alpha::R27)
            .Case("$at", Alpha::R28)
            .Case("$gp", Alpha::R29)
            .Case("$sp", Alpha::R30)
            .Case("$zero", Alpha::R31)
            .Default(MCRegister());
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
    // These are the names GNU as's .arch takes.  Its cpu_types table
    // (gas/config/tc-alpha.c) also holds chip numbers such as 21264, but those
    // reach it only through the -m command line: .arch reads a symbol name, so
    // a leading digit is rejected there, and we reject it too.
    //
    // What each name permits comes from that table.  Its CIX bit gates
    // ctpop/ctlz/cttz and itoft/ftoit/sqrtt alike, so it maps to both of our
    // features; its MAX bit is our MVI.  Note ev6 grants CIX there even though
    // the 21264 chip has only the FIX half -- .arch says what the assembler
    // will accept, not what the part implements, which is -mcpu's job.
    SmallVector<StringRef, 4> Feats;
    if (Arch == "ev4" || Arch == "ev45" || Arch == "lca45" || Arch == "ev5" ||
        Arch == "all") {
      // Base ISA only.
    } else if (Arch == "ev56") {
      Feats = {"bwx"};
    } else if (Arch == "pca56") {
      Feats = {"bwx", "mvi"};
    } else if (Arch == "ev6" || Arch == "ev67" || Arch == "ev68") {
      Feats = {"bwx", "cix", "fix", "mvi"};
    } else {
      // GNU as warns and falls back to the base ISA here.  An error is more
      // useful: the instructions the name was meant to enable would fail to
      // assemble a line later anyway.
      return Error(Loc, "unknown Alpha architecture '" + Arch + "'");
    }
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

  // ECOFF/OSF procedure-descriptor directives.  Most are hand-written-assembly
  // bookkeeping the ELF object does not need, but .ent names a procedure and
  // .prologue records how it establishes the global pointer, which the linker
  // needs when a caller reaches it with `!samegp`.  .end in particular must be
  // caught here, ahead of the generic directive that would stop assembly.
  StringRef ID = DirectiveID.getIdentifier();
  if (ID == ".ent") {
    StringRef Name;
    if (getParser().parseIdentifier(Name))
      return Error(getParser().getTok().getLoc(),
                   "expected symbol name after .ent");
    CurEntSym = getContext().getOrCreateSymbol(Name);
    // Mark the symbol as a function, matching GAS behavior.
    static_cast<MCSymbolELF *>(CurEntSym)->setType(ELF::STT_FUNC);
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  if (ID == ".prologue") {
    int64_t Arg;
    if (getParser().parseAbsoluteExpression(Arg))
      return ParseStatus::Failure;
    getParser().eatToEndOfStatement();
    // .prologue 0 marks a routine that needs no procedure value (it runs on the
    // caller's gp: STO_ALPHA_NOPV); .prologue 1 marks the standard two-word gp
    // load a same-gp caller may skip (STO_ALPHA_STD_GPLOAD).
    if (CurEntSym) {
      auto *Sym = static_cast<MCSymbolELF *>(CurEntSym);
      const unsigned STO_ALPHA_NOPV = 0x80, STO_ALPHA_STD_GPLOAD = 0x88;
      unsigned Other = Sym->getOther() & ~STO_ALPHA_STD_GPLOAD;
      if (Arg == 0)
        Other |= STO_ALPHA_NOPV;
      else if (Arg == 1)
        Other |= STO_ALPHA_STD_GPLOAD;
      Sym->setOther(Other);
    }
    return ParseStatus::Success;
  }
  if (ID == ".end") {
    CurEntSym = nullptr;
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  if (ID == ".frame" || ID == ".mask" || ID == ".fmask") {
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  // `.usepv sym, std|no` sets the STO_ALPHA_STD_GPLOAD / STO_ALPHA_NOPV bit on
  // sym so the linker can optimize same-gp calls (R_ALPHA_BRSGP).  Used by
  // hand-written assembly that defines functions without .ent/.prologue.
  if (ID == ".usepv") {
    const unsigned STO_ALPHA_NOPV = 0x80, STO_ALPHA_STD_GPLOAD = 0x88;
    StringRef SymName;
    if (getParser().parseIdentifier(SymName))
      return Error(DirectiveID.getLoc(), "expected symbol name after .usepv");
    if (getLexer().isNot(AsmToken::Comma))
      return Error(getLexer().getLoc(), "expected ',' after symbol name");
    getParser().Lex(); // ,
    StringRef Mode;
    if (getParser().parseIdentifier(Mode))
      return Error(getLexer().getLoc(), "expected 'std' or 'no'");
    unsigned Other;
    if (Mode == "std")
      Other = STO_ALPHA_STD_GPLOAD;
    else if (Mode == "no")
      Other = STO_ALPHA_NOPV;
    else
      return Error(getLexer().getLoc(), "unknown .usepv mode '" + Mode + "'");
    MCSymbol *Sym = getContext().getOrCreateSymbol(SymName);
    auto *SymELF = static_cast<MCSymbolELF *>(Sym);
    SymELF->setOther((SymELF->getOther() & ~STO_ALPHA_STD_GPLOAD) | Other);
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  // `.gprel32 sym` emits a 32-bit GP-relative value (R_ALPHA_GPREL32).
  if (ID == ".gprel32") {
    const MCExpr *Expr;
    if (getParser().parseExpression(Expr))
      return ParseStatus::Failure;
    getParser().eatToEndOfStatement();
    // There is no `!gprel32' suffix to print: the `!' grammar attaches a
    // relocation specifier to an instruction operand, never to a data
    // directive.  So assembly output echoes the directive itself, the way
    // AlphaAsmPrinter::emitJumpTableEntry writes a jump-table entry; emitting
    // the value would print a bare `.long', which reassembles to
    // R_ALPHA_REFLONG and turns a GP-relative table into an absolute one.
    if (getStreamer().hasRawTextSupport()) {
      SmallString<128> Str;
      raw_svector_ostream OS(Str);
      OS << "\t.gprel32\t";
      getContext().getAsmInfo().printExpr(OS, *Expr);
      getStreamer().emitRawText(OS.str());
      return ParseStatus::Success;
    }
    const MCExpr *GPRel =
        MCSpecifierExpr::create(Expr, Alpha::fixup_alpha_gprel32, getContext());
    getStreamer().emitValue(GPRel, 4, DirectiveID.getLoc());
    return ParseStatus::Success;
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
  bool Matched = !matchRegister(("$" + Name).str(), Reg);
  // GNU as also spells integer register $N as "$rN".
  if (!Matched && Name.size() > 1 && Name[0] == 'r' &&
      llvm::all_of(Name.drop_front(), isDigit))
    Matched = !matchRegister(("$" + Name.drop_front()).str(), Reg);
  if (!Matched)
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

  // A leading '$' names a register ($<number>/$<name>).  If the name is not a
  // register it is a '$'-prefixed local label used in an operand expression --
  // a branch target (`bne $1, $target`) or a displacement (`$exc-99b($16)`);
  // fall through to the expression parser, which accepts '$' in identifiers.
  if (getParser().getTok().is(AsmToken::Dollar)) {
    MCRegister Reg;
    if (tryParseRegister(Reg, S, E).isSuccess()) {
      Operands.push_back(AlphaOperand::createReg(Reg, S, E));
      return ParseStatus::Success;
    }
  }

  // An expression: either a bare immediate or the displacement of a memory
  // operand disp($base).  A leading '(' introduces the base register with an
  // implicit zero displacement (`($base)`) only when a register follows it;
  // otherwise it opens a parenthesized displacement expression such as
  // `(1<<3)($base)`.
  const MCExpr *Off;
  if (getParser().getTok().is(AsmToken::LParen) &&
      getLexer().peekTok().is(AsmToken::Dollar)) {
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
  // Alpha FP instructions can carry a qualifier suffix: divt/c, cvttq/c, etc.
  // The lexer splits "divt/c" into three tokens (divt, /, c), so we must
  // reassemble the full mnemonic before looking it up.
  PendingFPQual = 0;
  PendingFPQualIsV = false;
  PendingFPQualBase = StringRef();
  StringRef Mnem = Name;
  if (getLexer().is(AsmToken::Slash)) {
    SMLoc SlashLoc = getLexer().getLoc();
    getParser().Lex(); // /
    if (getLexer().is(AsmToken::Identifier)) {
      // Build "base/qualifier" and intern it in the context's bump allocator
      // so the StringRef stored in the token operand remains valid after this
      // function returns (createToken stores a raw pointer, not a copy).
      StringRef Qual = getLexer().getTok().getIdentifier();
      SmallString<16> Buf;
      Buf += Name;
      Buf += '/';
      Buf += Qual;
      Mnem = getParser().getContext().allocateString(Buf);
      getParser().Lex(); // qualifier

      // A trap qualifier, optionally followed by a rounding letter: su, sui,
      // sud, c, and so on.  Anything else is part of a mnemonic that is spelled
      // with its qualifier, such as cvttq/svid, and is matched as written.
      StringRef Trap = Qual;
      unsigned RM = Alpha::FPRoundNormal;
      if (Trap.size() > 1 || Trap == "c" || Trap == "m" || Trap == "d") {
        unsigned Cand = StringSwitch<unsigned>(Trap.take_back())
                            .Case("c", Alpha::FPRoundChopped)
                            .Case("m", Alpha::FPRoundMinus)
                            .Case("d", Alpha::FPRoundDynamic)
                            .Default(~0u);
        bool Ok = false;
        if (Cand != ~0u) {
          Alpha::getFPTrapFuncBitsForSpelling(Trap.drop_back(), Ok);
          if (Ok) {
            RM = Cand;
            Trap = Trap.drop_back();
          }
        }
      }
      bool Ok = false;
      unsigned TrapBits = Alpha::getFPTrapFuncBitsForSpelling(Trap, Ok);
      if (Ok && (TrapBits || RM != Alpha::FPRoundNormal)) {
        PendingFPQual = Alpha::encodeFPQual(TrapBits, RM);
        PendingFPQualIsV = Trap.contains('v');
        PendingFPQualBase = Name;
      }
    } else {
      return Error(SlashLoc, "expected qualifier after '/'");
    }
  }
  Operands.push_back(AlphaOperand::createToken(Mnem, NameLoc));

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
                        .Case("gprel", Alpha::fixup_alpha_gprel16)
                        .Case("gpdisp", Alpha::fixup_alpha_gpdisp)
                        .Case("tprelhi", Alpha::fixup_alpha_tprelhi)
                        .Case("tprello", Alpha::fixup_alpha_tprello)
                        .Case("gottprel", Alpha::fixup_alpha_gottprel)
                        .Case("tlsgd", Alpha::fixup_alpha_tlsgd)
                        .Case("tlsldm", Alpha::fixup_alpha_tlsldm)
                        .Case("dtprelhi", Alpha::fixup_alpha_dtprelhi)
                        .Case("dtprello", Alpha::fixup_alpha_dtprello)
                        .Case("samegp", Alpha::fixup_alpha_brsgp)
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

MCRegister AlphaAsmParser::emitConst32(MCRegister Rc, int32_t V32,
                                       MCRegister Base, SMLoc L,
                                       MCStreamer &Out) {
  auto emit = [&](unsigned Op, int64_t Imm, MCRegister Src) {
    MCInst I;
    I.setOpcode(Op);
    I.addOperand(MCOperand::createReg(Rc));
    I.addOperand(MCOperand::createImm(Imm));
    I.addOperand(MCOperand::createReg(Src));
    I.setLoc(L);
    Out.emitInstruction(I, getSTI());
  };
  // V32 = (Hi << 16) + Lo, Lo sign-extended.  Hi lies in [-0x8000, 0x8000]; the
  // +0x8000 case does not fit ldah's signed field, so emit two ldah of 0x4000.
  int64_t Lo = static_cast<int16_t>(V32);
  int64_t Hi = (static_cast<int64_t>(V32) - Lo) >> 16;
  MCRegister Cur = Base;
  if (Hi != 0) {
    if (isInt<16>(Hi)) {
      emit(Alpha::LDAH, Hi, Cur);
    } else {
      emit(Alpha::LDAH, Hi / 2, Cur);
      emit(Alpha::LDAH, Hi / 2, Rc);
    }
    Cur = Rc;
  }
  if (Lo != 0 || Cur == Base) {
    emit(Alpha::LDA, Lo, Cur);
    Cur = Rc;
  }
  return Cur;
}

void AlphaAsmParser::emitLoadImm(MCRegister Rc, int64_t V, SMLoc L,
                                 MCStreamer &Out) {
  auto emit1 = [&](unsigned Op, ArrayRef<MCOperand> Ops) {
    MCInst I;
    I.setOpcode(Op);
    for (const MCOperand &O : Ops)
      I.addOperand(O);
    I.setLoc(L);
    Out.emitInstruction(I, getSTI());
  };
  // A 16-bit value is a single lda; a value that fits 32 bits is an ldah/lda
  // pair (with a zapnot to clear the sign extension of an unsigned 32-bit value
  // whose bit 31 is set).
  if (SignExtend64<16>(V) == V) {
    emit1(Alpha::LDAi, {MCOperand::createReg(Rc), MCOperand::createImm(V)});
    return;
  }
  if (isInt<32>(V) || isUInt<32>(V)) {
    emitConst32(Rc, static_cast<int32_t>(V), Alpha::R31, L, Out);
    if (!isInt<32>(V))
      emit1(Alpha::ZAPNOTi, {MCOperand::createReg(Rc), MCOperand::createReg(Rc),
                             MCOperand::createImm(0xf)});
    return;
  }
  // Wider: build the high half, shift it up by 32, then add the low half; the
  // sign of the low half is already folded into the high half.
  int32_t Lo32 = static_cast<int32_t>(V);
  emitConst32(Rc, static_cast<int32_t>((V - Lo32) >> 32), Alpha::R31, L, Out);
  emit1(Alpha::SLLi, {MCOperand::createReg(Rc), MCOperand::createReg(Rc),
                      MCOperand::createImm(32)});
  if (Lo32 != 0)
    emitConst32(Rc, Lo32, Rc, L, Out);
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

  // jsr $Ra, symbol: an indirect call to a symbol reached through the GOT (GNU
  // as's jsr-to-symbol macro).  Load the target's address from its GOT entry
  // into $27 (the procedure value) and jsr through it.
  if (Mnemonic == "jsr" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm() &&
      !isa<MCConstantExpr>(
          static_cast<AlphaOperand &>(*Operands[2]).getImm())) {
    MCRegister Ra = static_cast<AlphaOperand &>(*Operands[1]).getReg();
    const MCExpr *Sym = static_cast<AlphaOperand &>(*Operands[2]).getImm();
    MCInst Ptr; // ldq $27, symbol($gp) !literal
    Ptr.setOpcode(Alpha::LDQl);
    Ptr.addOperand(MCOperand::createReg(Alpha::R27));
    Ptr.addOperand(MCOperand::createExpr(Sym));
    Ptr.setLoc(IDLoc);
    Out.emitInstruction(Ptr, getSTI());
    MCInst Call; // jsr $Ra, ($27)
    Call.setOpcode(Alpha::JSRr);
    Call.addOperand(MCOperand::createReg(Ra));
    Call.addOperand(MCOperand::createReg(Alpha::R27));
    Call.setLoc(IDLoc);
    Out.emitInstruction(Call, getSTI());
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

  // jmp $Ra, symbol: a jump to a symbol reached through the GOT, expanded like
  // jsr but discarding the return address.
  if (Mnemonic == "jmp" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm() &&
      !isa<MCConstantExpr>(
          static_cast<AlphaOperand &>(*Operands[2]).getImm())) {
    const MCExpr *Sym = static_cast<AlphaOperand &>(*Operands[2]).getImm();
    MCInst Ptr; // ldq $27, symbol($gp) !literal
    Ptr.setOpcode(Alpha::LDQl);
    Ptr.addOperand(MCOperand::createReg(Alpha::R27));
    Ptr.addOperand(MCOperand::createExpr(Sym));
    Ptr.setLoc(IDLoc);
    Out.emitInstruction(Ptr, getSTI());
    MCInst Jmp; // jmp $31, ($27)
    Jmp.setOpcode(Alpha::JMP);
    Jmp.addOperand(MCOperand::createReg(Alpha::R27));
    Jmp.setLoc(IDLoc);
    Out.emitInstruction(Jmp, getSTI());
    return false;
  }

  // jsr $Ra, ($Rb), hint: a computed call whose third operand is a
  // branch-prediction hint (an R_ALPHA_HINT we do not need to emit).
  if (Mnemonic == "jsr" && Operands.size() == 4 && Operands[1]->isReg() &&
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

  // ldq/ldl/ldbu/ldwu $R, symbol: in large-data (GOT) mode GNU as expands a
  // load from a bare symbol into a load of the symbol's GOT entry (its address)
  // followed by a dereference of the requested width.  (GNU as also tags the
  // second load with an R_ALPHA_LITUSE relaxation hint; omitting it costs an
  // optimization, not correctness.)
  unsigned DerefOp = StringSwitch<unsigned>(Mnemonic)
                         .Case("ldq", Alpha::LDQ)
                         .Case("ldl", Alpha::LDL)
                         .Case("ldbu", Alpha::LDBU)
                         .Case("ldwu", Alpha::LDWU)
                         .Default(0);
  if (DerefOp && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm()) {
    const MCExpr *Sym = static_cast<AlphaOperand &>(*Operands[2]).getImm();
    if (!isa<MCConstantExpr>(Sym)) {
      // The dereference is a real instruction and has to be available.  GNU as
      // expands the byte and word cases into an ldq_u/ext pair when the target
      // has no BWX; we do not implement that macro, so refuse rather than emit
      // an instruction the target cannot execute -- which is what the matcher
      // does for the `ldbu $0, 0($16)' spelling of the same load.
      if ((DerefOp == Alpha::LDBU || DerefOp == Alpha::LDWU) &&
          !getSTI().hasFeature(Alpha::FeatureBWX))
        return Error(IDLoc, "instruction requires the following: "
                            "Byte/word extension (BWX)");
      MCRegister R = static_cast<AlphaOperand &>(*Operands[1]).getReg();
      MCInst Ptr; // ldq $R, symbol($gp) !literal  (the GOT slot is a quadword)
      Ptr.setOpcode(Alpha::LDQl);
      Ptr.addOperand(MCOperand::createReg(R));
      Ptr.addOperand(MCOperand::createExpr(Sym));
      Ptr.setLoc(IDLoc);
      Out.emitInstruction(Ptr, getSTI());
      MCInst Deref; // <load> $R, 0($R)  (the load itself, of the given width)
      Deref.setOpcode(DerefOp);
      Deref.addOperand(MCOperand::createReg(R));
      Deref.addOperand(MCOperand::createReg(R));
      Deref.addOperand(MCOperand::createImm(0));
      Deref.setLoc(IDLoc);
      Out.emitInstruction(Deref, getSTI());
      return false;
    }
  }

  // lda $R, symbol: the address of a symbol is its GOT entry, so GNU as loads
  // it directly (like the ldq form but without the dereference).  A base
  // register (lda $R, disp($base)) or a constant makes this an ordinary lda
  // instead.
  if (Mnemonic == "lda" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm()) {
    const MCExpr *Sym = static_cast<AlphaOperand &>(*Operands[2]).getImm();
    if (!isa<MCConstantExpr>(Sym)) {
      MCInst Ptr; // ldq $R, symbol($gp) !literal
      Ptr.setOpcode(Alpha::LDQl);
      Ptr.addOperand(MCOperand::createReg(
          static_cast<AlphaOperand &>(*Operands[1]).getReg()));
      Ptr.addOperand(MCOperand::createExpr(Sym));
      Ptr.setLoc(IDLoc);
      Out.emitInstruction(Ptr, getSTI());
      return false;
    }
  }

  // lda $Rc, disp($Rb) where disp is a symbol (whose address comes from the
  // GOT) or a constant too wide for the 16-bit field: load or materialize disp
  // into $Rc, then add the base.  The base must survive the add, so $Rc cannot
  // be the base -- which under `.set noat` (no scratch) GNU as also cannot
  // handle.
  if (Mnemonic == "lda" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isMem()) {
    const MCExpr *Off = static_cast<AlphaOperand &>(*Operands[2]).getMemOff();
    auto *CE = dyn_cast<MCConstantExpr>(Off);
    // A bare symbol reference is a GOT address; a wide constant is
    // materialized. A symbol difference, a relocation specifier, or a small
    // constant is an ordinary lda, left to the matcher.
    //
    // `sym+N' is the GOT address plus a constant.  GNU as accepts it, and it
    // has to be taken apart here: the literal relocation names the symbol, so
    // leaving the addend in the expression would either drop it or ask for a
    // GOT entry for sym+N that the linker will not make.
    const MCExpr *Sub = Off;
    int64_t Addend = 0;
    if (const auto *BE = dyn_cast<MCBinaryExpr>(Off)) {
      const auto *RHS = dyn_cast<MCConstantExpr>(BE->getRHS());
      if (RHS && isa<MCSymbolRefExpr>(BE->getLHS()) &&
          (BE->getOpcode() == MCBinaryExpr::Add ||
           BE->getOpcode() == MCBinaryExpr::Sub)) {
        Sub = BE->getLHS();
        Addend = BE->getOpcode() == MCBinaryExpr::Add ? RHS->getValue()
                                                      : -RHS->getValue();
      }
    }
    bool BigConst = CE && !isInt<16>(CE->getValue());
    bool Sym = isa<MCSymbolRefExpr>(Sub);
    if (BigConst || Sym) {
      MCRegister Rc = static_cast<AlphaOperand &>(*Operands[1]).getReg();
      MCRegister Rb = static_cast<AlphaOperand &>(*Operands[2]).getMemBase();
      if (Rc == Rb)
        return Error(IDLoc, "lda of this displacement needs a scratch register "
                            "distinct from the base");
      if (BigConst) {
        emitLoadImm(Rc, CE->getValue(), IDLoc, Out);
      } else {
        MCInst Ptr; // ldq $Rc, symbol($gp) !literal
        Ptr.setOpcode(Alpha::LDQl);
        Ptr.addOperand(MCOperand::createReg(Rc));
        Ptr.addOperand(MCOperand::createExpr(Sub));
        Ptr.setLoc(IDLoc);
        Out.emitInstruction(Ptr, getSTI());
        if (Addend) {
          if (!isInt<16>(Addend))
            return Error(IDLoc, "lda addend does not fit a 16-bit "
                                "displacement");
          MCInst Off2; // lda $Rc, Addend($Rc)
          Off2.setOpcode(Alpha::LDA);
          Off2.addOperand(MCOperand::createReg(Rc));
          Off2.addOperand(MCOperand::createImm(Addend));
          Off2.addOperand(MCOperand::createReg(Rc));
          Off2.setLoc(IDLoc);
          Out.emitInstruction(Off2, getSTI());
        }
      }
      // $31 reads as zero, so adding it changes nothing; GNU as leaves the add
      // out rather than emitting a no-op.
      if (Rb != Alpha::R31) {
        MCInst Add; // addq $Rc, $Rb, $Rc
        Add.setOpcode(Alpha::ADDQ);
        Add.addOperand(MCOperand::createReg(Rc));
        Add.addOperand(MCOperand::createReg(Rc));
        Add.addOperand(MCOperand::createReg(Rb));
        Add.setLoc(IDLoc);
        Out.emitInstruction(Add, getSTI());
      }
      return false;
    }
  }

  // mov imm, $Rc: GNU as defines this as `bis $31, imm, $Rc' and nothing else
  // -- the operate instruction's own 8-bit unsigned literal field.  It is not
  // the ldi materialization macro: a value that does not fit is an error
  // ("operand out of range"), not a longer sequence, and even a value that
  // does fit assembles to a different instruction from the one lda gives.
  if (Mnemonic == "mov" && Operands.size() == 3 && Operands[1]->isImm() &&
      Operands[2]->isReg()) {
    const MCExpr *E = static_cast<AlphaOperand &>(*Operands[1]).getImm();
    if (auto *CE = dyn_cast<MCConstantExpr>(E)) {
      int64_t V = CE->getValue();
      if (!isUInt<8>(V))
        return Error(Operands[1]->getStartLoc(),
                     "operand out of range (" + Twine(V) +
                         " is not between 0 and 255)");
      MCInst Inst;
      Inst.setOpcode(Alpha::BISi);
      Inst.addOperand(MCOperand::createReg(
          static_cast<AlphaOperand &>(*Operands[2]).getReg()));
      Inst.addOperand(MCOperand::createReg(Alpha::R31));
      Inst.addOperand(MCOperand::createImm(V));
      Inst.setLoc(IDLoc);
      Out.emitInstruction(Inst, getSTI());
      return false;
    }
  }

  // ldi/ldiq $Rc, imm: load an immediate constant, materializing it in code.
  if ((Mnemonic == "ldi" || Mnemonic == "ldiq") && Operands.size() == 3 &&
      Operands[1]->isReg() && Operands[2]->isImm()) {
    const MCExpr *E = static_cast<AlphaOperand &>(*Operands[2]).getImm();
    if (auto *CE = dyn_cast<MCConstantExpr>(E)) {
      emitLoadImm(static_cast<AlphaOperand &>(*Operands[1]).getReg(),
                  CE->getValue(), IDLoc, Out);
      return false;
    }
  }

  MCInst Inst;
  unsigned Result =
      MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
  unsigned Flags = 0;
  if (Result != Match_Success && PendingFPQual) {
    // Most qualified operates have no def of their own -- `addt/su' is `addt'
    // with bits in its function field -- so match the base mnemonic and carry
    // the qualifier alongside.  The few that are spelled out, such as
    // cvttq/svid, matched above and keep their own encoding.
    Operands[0] = AlphaOperand::createToken(PendingFPQualBase, IDLoc);
    MCInst Retry;
    unsigned R2 =
        MatchInstructionImpl(Operands, Retry, ErrorInfo, MatchingInlineAsm);
    if (R2 == Match_Success) {
      // Only an instruction that has a qualifier field can be given one.
      unsigned TrapClass = MII.get(Retry.getOpcode()).TSFlags &
                           Alpha::TrapClassMask;
      if (!TrapClass)
        return Error(IDLoc, "instruction does not take a floating-point "
                            "qualifier");
      // And only one of the combinations its class defines.  The function
      // field has room for spellings no instruction has -- a rounding letter
      // on a compare, an underflow bit without inexact on cvtqt -- and merging
      // one in produces a word that is reserved at best and, for cvtst, reads
      // back as a different real instruction.
      if (!Alpha::fpQualIsLegal(TrapClass,
                                Alpha::fpQualTrapBits(PendingFPQual),
                                Alpha::fpQualRoundMode(PendingFPQual)) ||
          !Alpha::fpTrapSpellingMatchesClass(
              TrapClass, Alpha::fpQualTrapBits(PendingFPQual),
              PendingFPQualIsV))
        return Error(IDLoc, "invalid floating-point qualifier for this "
                            "instruction");
      Inst = Retry;
      Result = R2;
      Flags = PendingFPQual;
    }
  }
  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    // Whatever was written is what this instruction carries -- including
    // nothing, which is a qualifier too.  Recording it stops -mieee from
    // turning a written `addt' into `addt/su'.  A mnemonic spelled with its
    // qualifier, such as cvttq/svid, already has it in the encoding and needs
    // no flag; it has TrapClass 0 for that reason.
    if (MII.get(Inst.getOpcode()).TSFlags & Alpha::TrapClassMask)
      Inst.setFlags(Flags ? Flags
                          : Alpha::encodeFPQual(0, Alpha::FPRoundNormal));
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
