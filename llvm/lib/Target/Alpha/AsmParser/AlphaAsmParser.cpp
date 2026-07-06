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

  // What the ECOFF procedure directives said about one procedure.  GNU as
  // keeps this to the end of the file and synthesises .eh_frame from it there
  // (alpha_elf_md_finish), so that hand-written assembly -- glibc's setjmp,
  // __longjmp, start and the string routines, the dynamic linker's trampoline
  // -- gets unwind information without writing .cfi_* directives.
  struct EntFrame {
    MCSymbol *Begin = nullptr;
    MCSymbol *End = nullptr;
    // Where the prologue is complete.  Set by `.prologue', and its absence is
    // what says this procedure gets no frame description at all.
    MCSymbol *Prologue = nullptr;
    unsigned Mask = 0;
    unsigned FMask = 0;
    // DWARF register numbers, defaulted as GNU as defaults them.
    unsigned FPReg = 30;
    unsigned RAReg = 26;
    int64_t FrameSize = 0;
    int64_t MaskOffset = 0;
    int64_t FMaskOffset = 0;
  };
  std::vector<EntFrame> EntFrames;
  // Index into EntFrames of the procedure between `.ent' and `.end', or -1.
  int CurEntFrame = -1;

  // A frame register as GNU as reads one (tc_get_register): written `$N',
  // and anything else warns and is taken as the stack pointer.
  unsigned parseFrameRegister() {
    MCRegister Reg;
    SMLoc S, E;
    if (getLexer().is(AsmToken::Dollar) &&
        tryParseRegister(Reg, S, E).isSuccess()) {
      int N = getContext().getRegisterInfo()->getDwarfRegNum(Reg, true);
      if (N >= 0)
        return N;
    }
    Warning(getLexer().getLoc(), "frame reg expected, using $30");
    return 30;
  }

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

  void onEndOfFile() override;

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

  // Add a 32-bit signed value to Base with ldah/lda, writing the result to Rc;
  // returns the register now holding it (Rc, or Base if nothing was emitted).
  void emitConstantSteps(MCRegister Rc, ArrayRef<Alpha::ConstantStep> Steps,
                         MCRegister Base, SMLoc L, MCStreamer &Out);
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
    // Start collecting the procedure's frame.  A local label rather than the
    // procedure's own symbol, so that .eh_frame needs no relocation against a
    // global -- GNU as does the same, and for the same reason.
    if (!getStreamer().hasRawTextSupport()) {
      EntFrame F;
      F.Begin = getContext().createTempSymbol("ent");
      getStreamer().emitLabel(F.Begin);
      CurEntFrame = EntFrames.size();
      EntFrames.push_back(F);
    }
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
    // The frame description takes effect here: everything .frame and .mask
    // describe is in place once the prologue is done.
    if (CurEntFrame >= 0) {
      EntFrames[CurEntFrame].Prologue = getContext().createTempSymbol("prol");
      getStreamer().emitLabel(EntFrames[CurEntFrame].Prologue);
    }
    return ParseStatus::Success;
  }
  if (ID == ".end") {
    // GNU as gives the procedure its st_size here, from .ent to .end.  Without
    // it every function gcc compiles is a FUNC of size 0, which leaves gdb and
    // the profilers unable to say which function an address is in, and makes
    // `nm --size-sort' and ld's --gc-sections warnings meaningless.
    // Assembly output echoes .end as it was written and an assembler reading
    // it back does this then, so adding a label and a .size there would be
    // noise.
    if (CurEntSym && !getStreamer().hasRawTextSupport()) {
      MCSymbol *EndSym = getContext().createTempSymbol("end");
      getStreamer().emitLabel(EndSym);
      static_cast<MCSymbolELF *>(CurEntSym)
          ->setSize(MCBinaryExpr::createSub(
              MCSymbolRefExpr::create(EndSym, getContext()),
              MCSymbolRefExpr::create(CurEntSym, getContext()), getContext()));
      if (CurEntFrame >= 0)
        EntFrames[CurEntFrame].End = EndSym;
    }
    CurEntFrame = -1;
    CurEntSym = nullptr;
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  if (ID == ".frame") {
    // .frame <frame register>, <frame size>, <return-address register>
    //        [, <offset of the saved $a0>]
    // The last operand has nowhere to go in an ELF object; GNU as ignores it.
    EntFrame Discard;
    EntFrame &F = CurEntFrame >= 0 ? EntFrames[CurEntFrame] : Discard;
    if (CurEntFrame < 0 && !getStreamer().hasRawTextSupport())
      Warning(DirectiveID.getLoc(), ".frame outside of .ent");
    F.FPReg = parseFrameRegister();
    int64_t Size;
    if (!getParser().parseOptionalToken(AsmToken::Comma) ||
        getParser().parseAbsoluteExpression(Size) ||
        !getParser().parseOptionalToken(AsmToken::Comma)) {
      Warning(DirectiveID.getLoc(), "bad .frame directive");
      getParser().eatToEndOfStatement();
      return ParseStatus::Success;
    }
    F.FrameSize = Size;
    F.RAReg = parseFrameRegister();
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  if (ID == ".mask" || ID == ".fmask") {
    // .mask <bitmask of saved registers>, <offset of the first from the CFA>
    int64_t Val, Offset;
    if (getParser().parseAbsoluteExpression(Val) ||
        !getParser().parseOptionalToken(AsmToken::Comma) ||
        getParser().parseAbsoluteExpression(Offset)) {
      Warning(DirectiveID.getLoc(), Twine("bad ") + ID + " directive");
      getParser().eatToEndOfStatement();
      return ParseStatus::Success;
    }
    if (CurEntFrame >= 0) {
      EntFrame &F = EntFrames[CurEntFrame];
      if (ID == ".mask") {
        F.Mask = Val;
        F.MaskOffset = Offset;
      } else {
        F.FMask = Val;
        F.FMaskOffset = Offset;
      }
    }
    getParser().eatToEndOfStatement();
    return ParseStatus::Success;
  }
  if (ID == ".usepv") {
    getParser().eatToEndOfStatement();
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

// Synthesise .eh_frame from the ECOFF procedure directives, as GNU as does at
// the end of assembly (alpha_elf_md_finish).  Doing it here rather than as the
// directives are read is what makes the two rules below possible: the frame
// covers the whole procedure although nothing is known about it until `.end',
// and a file that described its frames itself is left alone.
void AlphaAsmParser::onEndOfFile() {
  // Assembly output passes the directives through unchanged; an assembler
  // reading it back does this then.
  if (getStreamer().hasRawTextSupport())
    return;
  // If anything in the file wrote .cfi_* directives, that is the description
  // of record and these directives are only bookkeeping beside it.  gcc emits
  // both, which is why this is a whole-file decision and not a per-procedure
  // one.
  if (!getStreamer().getDwarfFrameInfos().empty())
    return;

  for (const EntFrame &F : EntFrames) {
    // No `.prologue' means the procedure never said where its frame is set
    // up, so there is nothing to describe.  glibc's ENTRY macro is like this,
    // and GNU as gives those procedures no FDE either.
    if (!F.Prologue || !F.Begin || !F.End)
      continue;
    SmallVector<MCCFIInstruction, 8> Instrs;
    // A procedure that neither moves the stack pointer nor saves a register
    // keeps the CIE's rule, CFA = $30, for its whole length.
    if (F.FPReg != 30 || F.Mask || F.FMask || F.FrameSize) {
      if (F.FPReg != 30) {
        if (F.FrameSize)
          Instrs.push_back(
              MCCFIInstruction::cfiDefCfa(F.Prologue, F.FPReg, F.FrameSize));
        else
          Instrs.push_back(
              MCCFIInstruction::createDefCfaRegister(F.Prologue, F.FPReg));
      } else if (F.FrameSize) {
        Instrs.push_back(
            MCCFIInstruction::cfiDefCfaOffset(F.Prologue, F.FrameSize));
      }
      // The saved registers sit at consecutive slots from the offset the mask
      // gives, in register order -- except the return address, which is
      // stored first whatever its number.
      unsigned Mask = F.Mask;
      int64_t Offset = F.MaskOffset;
      if (Mask & (1u << 26)) {
        Instrs.push_back(MCCFIInstruction::createOffset(F.Prologue, 26, Offset));
        Offset += 8;
        Mask &= ~(1u << 26);
      }
      while (Mask) {
        unsigned I = llvm::countr_zero(Mask);
        Mask &= Mask - 1;
        Instrs.push_back(MCCFIInstruction::createOffset(F.Prologue, I, Offset));
        Offset += 8;
      }
      // The floating-point registers are DWARF numbers 32 and up.
      Mask = F.FMask;
      Offset = F.FMaskOffset;
      while (Mask) {
        unsigned I = llvm::countr_zero(Mask);
        Mask &= Mask - 1;
        Instrs.push_back(
            MCCFIInstruction::createOffset(F.Prologue, I + 32, Offset));
        Offset += 8;
      }
    }
    getStreamer().emitCFIFrame(F.Begin, F.End, F.RAReg, Instrs);
  }
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

void AlphaAsmParser::emitConstantSteps(MCRegister Rc,
                                       ArrayRef<Alpha::ConstantStep> Steps,
                                       MCRegister Base, SMLoc L,
                                       MCStreamer &Out) {
  MCRegister Cur = Base;
  for (const Alpha::ConstantStep &S : Steps) {
    MCInst I;
    I.setOpcode(S.Opc);
    I.addOperand(MCOperand::createReg(Rc));
    // sll takes its register operand first, ldah and lda their displacement.
    if (S.Opc == Alpha::SLLi) {
      I.addOperand(MCOperand::createReg(Cur));
      I.addOperand(MCOperand::createImm(S.Imm));
    } else {
      I.addOperand(MCOperand::createImm(S.Imm));
      I.addOperand(MCOperand::createReg(Cur));
    }
    I.setLoc(L);
    Out.emitInstruction(I, getSTI());
    // Every step after the first reads what the one before it wrote.
    Cur = Rc;
  }
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
  // whose bit 31 is set).  Anything wider is built from both halves, which is
  // what buildConstantSteps does on its own.
  if (SignExtend64<16>(V) == V) {
    emit1(Alpha::LDAi, {MCOperand::createReg(Rc), MCOperand::createImm(V)});
    return;
  }
  SmallVector<Alpha::ConstantStep, 8> Steps;
  bool Fits32 = isInt<32>(V) || isUInt<32>(V);
  if (Fits32)
    Alpha::buildConstant32Steps(static_cast<int32_t>(V), Steps);
  else
    Alpha::buildConstantSteps(V, Steps);
  emitConstantSteps(Rc, Steps, Alpha::R31, L, Out);
  if (Fits32 && !isInt<32>(V))
    emit1(Alpha::ZAPNOTi, {MCOperand::createReg(Rc), MCOperand::createReg(Rc),
                           MCOperand::createImm(0xf)});
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
  //
  // `lda $Rc, disp' with the base left off is the same macro with $31 for the
  // base, and GNU as expands it the same way.  (A symbolic disp in that
  // spelling is the GOT load just above, so only a wide constant gets here.)
  if (Mnemonic == "lda" && Operands.size() == 3 && Operands[1]->isReg() &&
      (Operands[2]->isMem() || Operands[2]->isImm())) {
    bool HasBase = Operands[2]->isMem();
    const MCExpr *Off =
        HasBase ? static_cast<AlphaOperand &>(*Operands[2]).getMemOff()
                : static_cast<AlphaOperand &>(*Operands[2]).getImm();
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
      MCRegister Rb =
          HasBase ? static_cast<AlphaOperand &>(*Operands[2]).getMemBase()
                  : Alpha::R31;
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
