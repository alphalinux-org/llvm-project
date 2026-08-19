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
#include "llvm/ADT/APFloat.h"
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
  // instructions consume as a plain base register.  The displacement has to be
  // absent, because there is nowhere in the encoding to put one: matching
  // `8($3)' here would assemble it as `($3)' and lose the 8.  GNU as rejects
  // those forms, so failing to match produces the same diagnosis.
  bool isParenReg() const {
    if (Kind != Memory)
      return false;
    const auto *CE = dyn_cast<MCConstantExpr>(Mem.Off);
    return CE && CE->getValue() == 0;
  }

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
  // A relocation goes into a field, and only a memory displacement or an
  // immediate is one.  Report a register or a token so the caller can say so
  // rather than dropping what was written.
  bool applySpecifier(unsigned Spec, MCContext &Ctx) {
    if (Kind == Memory)
      Mem.Off = MCSpecifierExpr::create(Mem.Off, Spec, Ctx);
    else if (Kind == Immediate)
      Imm.Val = MCSpecifierExpr::create(Imm.Val, Spec, Ctx);
    else
      return false;
    return true;
  }
  // Same as applySpecifier but uses a caller-supplied expression as the base
  // instead of the operand's current expression.  Used for !gpdisp!N pairs
  // where the addend must be the distance from the ldah to the paired lda.
  void applySpecifierWithExpr(unsigned Spec, const MCExpr *E, MCContext &Ctx) {
    if (Kind == Memory)
      Mem.Off = MCSpecifierExpr::create(E, Spec, Ctx);
    else if (Kind == Immediate)
      Imm.Val = MCSpecifierExpr::create(E, Spec, Ctx);
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

  // State for paired !gpdisp!N annotations.
  //
  // Alpha assembly may write the ldah and lda halves of the GP setup sequence
  // far apart with other instructions between them:
  //   ldah $gp, 0($src)  !gpdisp!N   <- first occurrence
  //   ... intervening instructions ...
  //   lda  $gp, 0($gp)   !gpdisp!N   <- second occurrence
  //
  // The linker requires a single R_ALPHA_GPDISP on the ldah whose r_addend is
  // the byte distance to the lda.  GpDispLdaLabels maps the sequence number N
  // to a forward-reference MCSymbol that will be defined at the lda position.
  // PendingPreInsnLabels holds symbols to define just before the next
  // instruction is emitted (used to mark the ldah/lda positions).
  DenseMap<unsigned, MCSymbol *> GpDispLdaLabels;
  SmallVector<MCSymbol *, 2> PendingPreInsnLabels;

  // GNU as aligns a data item to its own width before emitting it
  // (alpha_cons_align, gas/config/tc-alpha.c), filling the padding the way the
  // section calls for: unop in an executable section, zeros elsewhere.
  void emitDataAlignment(Align A) {
    // Assembly output echoes the directives as they were written, and reading
    // it back runs it through this parser again, which aligns it then.  An
    // alignment printed here would be pure noise -- one line before every data
    // item -- so leave the text alone.
    if (getStreamer().hasRawTextSupport())
      return;
    MCSection *Sec = getStreamer().getCurrentSectionOnly();
    if (Sec && Sec->isText())
      getStreamer().emitCodeAlignment(A, getSTI());
    else
      getStreamer().emitValueToAlignment(A);
  }

  MCInst buildInst(SMLoc IDLoc, unsigned Opc, ArrayRef<MCOperand> Ops) {
    MCInst I;
    I.setOpcode(Opc);
    for (const MCOperand &O : Ops)
      I.addOperand(O);
    I.setLoc(IDLoc);
    return I;
  }

  // The operands the matcher hands us are all AlphaOperands; naming that once
  // keeps the macro expansions below readable.
  static AlphaOperand &op(const OperandVector &Operands, unsigned I) {
    return static_cast<AlphaOperand &>(*Operands[I]);
  }

  // Emit an instruction built here rather than by the matcher, carrying any
  // !lituse_* written on the line.  That relocation names the instruction it
  // is written on, so a line that expands into several gives it to the first
  // -- except the jsr/jmp-to-symbol macros, which hand it to the call they
  // end with, since the load they start with already has the literal.
  void emitInst(MCInst &I, MCStreamer &Out) {
    if (PendingLituse) {
      I.setFlags(I.getFlags() | Alpha::encodeLituse(PendingLituse - 1));
      PendingLituse = 0;
    }
    if (PendingSeq) {
      I.setFlags(I.getFlags() |
                 Alpha::encodeSeq(PendingSeq, PendingSeqIsLiteral));
      PendingSeq = 0;
    }
    Out.emitInstruction(I, getSTI());
  }

  // Build an instruction the parser assembles itself -- one of the macro
  // expansions below, or a piece of one -- and emit it through emitInst, so it
  // takes any !lituse_*/!literal!N written on the line.
  void emitAt(MCStreamer &Out, SMLoc IDLoc, unsigned Opc,
              ArrayRef<MCOperand> Ops) {
    MCInst I = buildInst(IDLoc, Opc, Ops);
    emitInst(I, Out);
  }

  // The same, but handed to the streamer directly.  A macro that opens with a
  // load of a GOT entry uses this for that load: the literal relocation is
  // already on it, and the pending relocation belongs to the instruction the
  // macro ends with.
  void emitAtRaw(MCStreamer &Out, SMLoc IDLoc, unsigned Opc,
                 ArrayRef<MCOperand> Ops) {
    MCInst I = buildInst(IDLoc, Opc, Ops);
    Out.emitInstruction(I, getSTI());
  }

  // The field a relocation specifier is written into, to be compared with the
  // one the matched encoding has.  Every GOT-, GP- and TLS-relative specifier
  // fills a 16-bit memory displacement; !samegp fills a 21-bit branch.
  static unsigned specifierRelocField(unsigned Spec) {
    if (Spec == Alpha::fixup_alpha_brsgp)
      return Alpha::RelocFieldBranch21;
    return Alpha::RelocFieldDisp16;
  }

  // The relocation specifier written on this instruction, checked against the
  // field the matched encoding actually has once it is known.
  unsigned PendingSpecifier = 0;
  SMLoc PendingSpecifierLoc;
  // The R_ALPHA_LITUSE use type a !lituse_* suffix asked for, biased by one so
  // that 0 can mean none: LITUSE_ALPHA_ADDR is use type 0.
  unsigned PendingLituse = 0;
  // The `!literal!N' / `!lituse_*!N' pair this instruction belongs to: the
  // dense id given to N (0 meaning none) and which half of the pair this is.
  // It exists only so the relocation table can be written with the pair
  // adjacent -- see AlphaELFObjectWriter::sortRelocs.
  unsigned PendingSeq = 0;
  bool PendingSeqIsLiteral = false;
  // The sequence number a `!literal!N' was written with, mapped to that dense
  // id.  A number can be reused once its pair is done, so a later literal
  // simply overwrites the entry.
  DenseMap<unsigned, unsigned> LiteralSeqIds;
  unsigned NextSeqId = 0;
  // Whether a data directive aligns itself first.  GNU as starts with it on and
  // `.align 0' turns it off, until the next `.align N' or section change.
  bool AutoAlignOn = true;
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

  // GNU as aligns every data item to its own width first, and hand-written
  // Alpha assembly leans on it: a .quad table written after an .asciz is
  // expected to start on an 8-byte boundary, and an ldq from it otherwise
  // faults.  The .2byte/.4byte/.8byte spellings are the explicitly unaligned
  // ones -- gas maps them to s_alpha_ucons, "Dwarf wants these versions" --
  // so they are left to the generic parser untouched, as is .byte.
  //
  // Only the alignment is emitted here; the values themselves are still the
  // generic parser's, which NoMatch hands them back to.
  {
    unsigned ConsAlign = StringSwitch<unsigned>(DirectiveID.getIdentifier())
                             .Cases({".short", ".value"}, 2u)
                             .Cases({".long", ".int"}, 4u)
                             .Case(".quad", 8u)
                             .Case(".octa", 16u)
                             .Default(0u);
    if (ConsAlign) {
      if (AutoAlignOn)
        emitDataAlignment(Align(ConsAlign));
      return ParseStatus::NoMatch;
    }
  }

  // `.align 0' turns auto-alignment off and emits nothing; any other `.align'
  // turns it back on and aligns (s_alpha_align).  A section change turns it
  // back on too, which is why the section directives are watched here.
  if (DirectiveID.getIdentifier() == ".align") {
    int64_t Alignment;
    SMLoc Loc = getParser().getTok().getLoc();
    if (getParser().parseAbsoluteExpression(Alignment))
      return ParseStatus::Failure;
    // GNU as takes an optional fill value, which it uses for the padding.
    // Nothing on Alpha writes one, and the section-appropriate fill this emits
    // is what the callers want, so accept and ignore it.
    if (getParser().parseOptionalToken(AsmToken::Comma)) {
      int64_t Fill;
      if (getParser().parseAbsoluteExpression(Fill))
        return ParseStatus::Failure;
    }
    if (getParser().parseEOL())
      return ParseStatus::Failure;
    if (Alignment == 0) {
      AutoAlignOn = false;
      return ParseStatus::Success;
    }
    if (Alignment < 0 || Alignment >= 32)
      return Error(Loc, "alignment must be between 0 and 31");
    AutoAlignOn = true;
    emitDataAlignment(Align(int64_t(1) << Alignment));
    return ParseStatus::Success;
  }
  if (StringSwitch<bool>(DirectiveID.getIdentifier())
          .Cases({".text", ".data", ".rodata", ".bss", ".section", ".pushsection",
                  ".popsection", ".previous"}, true)
          .Default(false)) {
    AutoAlignOn = true;
    return ParseStatus::NoMatch;
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
  // `.eflag <n>` sets flag bits in the ECOFF procedure descriptor; the asm
  // printer emits `.eflag 48' for -mieee-conformant, so the integrated
  // assembler has to read back what it writes.  The ELF object carries no
  // procedure descriptor for the bits to land in, so they are accepted and
  // dropped, as GNU as does for an ELF target.
  if (ID == ".eflag") {
    int64_t Flags;
    if (getParser().parseAbsoluteExpression(Flags))
      return ParseStatus::Failure;
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
    // A four-byte item, aligned like one (s_alpha_gprel32).
    if (AutoAlignOn)
      emitDataAlignment(Align(4));
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
  // `.s_floating` and `.t_floating` emit IEEE single and double values.  The
  // VAX formats the architecture also names (.f_floating, .d_floating and
  // .g_floating) are deliberately absent: nothing on Alpha Linux emits them.
  if (ID == ".s_floating" || ID == ".t_floating") {
    bool IsSingle = ID == ".s_floating";
    const fltSemantics &Sem =
        IsSingle ? APFloat::IEEEsingle() : APFloat::IEEEdouble();
    unsigned Size = IsSingle ? 4 : 8;
    // GNU as aligns the value to its own width first, and code that relies on
    // that does not write the .align itself.  It fills the padding the way the
    // section calls for: unop in an executable section, zeros elsewhere.  A
    // float constant normally lives in .rodata or .data, where writing a unop
    // would put an instruction in the data.
    if (AutoAlignOn)
      emitDataAlignment(Align(Size));
    do {
      bool Neg = getLexer().is(AsmToken::Minus);
      if (Neg || getLexer().is(AsmToken::Plus))
        getParser().Lex();
      if (getLexer().isNot(AsmToken::Real) &&
          getLexer().isNot(AsmToken::Integer))
        return Error(getLexer().getLoc(), "expected floating-point number");
      APFloat Value(Sem);
      if (llvm::Error E =
              Value
                  .convertFromString(getParser().getTok().getString(),
                                     APFloat::rmNearestTiesToEven)
                  .takeError()) {
        consumeError(std::move(E));
        return Error(getLexer().getLoc(), "invalid floating-point number");
      }
      getParser().Lex();
      if (Neg)
        Value.changeSign();
      getStreamer().emitIntValue(Value.bitcastToAPInt().getZExtValue(), Size);
    } while (getParser().parseOptionalToken(AsmToken::Comma));
    return getParser().parseEOL();
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
  // Alpha FP instructions can carry a qualifier suffix: divt/c, cvttq/c, etc.
  // The lexer splits "divt/c" into three tokens (divt, /, c), so we must
  // reassemble the full mnemonic before looking it up.
  PendingSpecifier = 0;
  PendingLituse = 0;
  PendingSeq = 0;
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
        unsigned Cand = Alpha::getFPRoundModeForSuffix(Trap.take_back());
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
    // The lituse relocations name no field: only the addend matters, and it
    // says which kind of use the instruction makes of the literal that came
    // before.  So one belongs to the instruction, not to any operand, and it
    // is attached to any instruction at all -- including a `jsr $26, ($27)'
    // written without the hint operand, where there is no expression for it to
    // sit on.  Carry the use type to matchAndEmitInstruction instead.
    // The full set GNU as accepts, with the LITUSE_ALPHA_* addends bfd reads
    // (include/elf/alpha.h).  lituse_jsrdirect is the one gcc emits itself, on
    // the millicode calls behind every integer / and %, so leaving it out
    // failed 16 of 237 gcc -O2 translation units outright.
    unsigned Lituse = StringSwitch<unsigned>(R)
                          .Case("lituse_addr", 0)
                          .Case("lituse_base", 1)
                          .Case("lituse_bytoff", 2)
                          .Case("lituse_jsr", 3)
                          .Case("lituse_tlsgd", 4)
                          .Case("lituse_tlsldm", 5)
                          .Case("lituse_jsrdirect", 6)
                          .Default(~0u);
    if (!Spec && Lituse == ~0u)
      return Error(getLexer().getLoc(), "unknown relocation name");
    SMLoc SpecLoc = getLexer().getLoc();
    getParser().Lex(); // name

    // Parse the optional !seq sequence number used to pair !gpdisp relocations
    // and to tie a !lituse_* to the !literal it uses.
    unsigned GpDispSeq = 0;
    bool HasSeq = false;
    if (getLexer().is(AsmToken::Exclaim)) {
      getParser().Lex(); // !
      if (getLexer().isNot(AsmToken::Integer))
        return Error(getLexer().getLoc(), "expected sequence number");
      GpDispSeq = getLexer().getTok().getIntVal();
      HasSeq = true;
      getParser().Lex(); // number
    }

    if (Lituse != ~0u) {
      // GNU as requires the number: it is how the use is tied to its literal,
      // and a use with no literal to relax against says nothing.
      if (!HasSeq)
        return Error(SpecLoc, "no sequence number after !" + R);
      // The number has to name a literal already seen; GNU as says so too, and
      // a use with nothing to relax against would silently do nothing.
      auto It = LiteralSeqIds.find(GpDispSeq);
      if (It == LiteralSeqIds.end())
        return Error(SpecLoc, "no !literal!" + Twine(GpDispSeq) + " was found");
      PendingSeq = It->second;
      PendingSeqIsLiteral = false;
      PendingLituse = Lituse + 1;
      if (getLexer().isNot(AsmToken::EndOfStatement))
        return Error(getLexer().getLoc(), "unexpected token");
      return false;
    }

    if (Spec == Alpha::fixup_alpha_literal && HasSeq) {
      // Give the number a dense id small enough to travel in the instruction
      // flags.  127 outstanding literals in one file is far past anything gas
      // or gcc writes; wrapping there costs an ordering, not correctness.
      NextSeqId = NextSeqId % 127 + 1;
      LiteralSeqIds[GpDispSeq] = NextSeqId;
      PendingSeq = NextSeqId;
      PendingSeqIsLiteral = true;
    }

    if (Spec == Alpha::fixup_alpha_gpdisp && GpDispSeq != 0) {
      auto It = GpDispLdaLabels.find(GpDispSeq);
      if (It == GpDispLdaLabels.end()) {
        // First occurrence: ldah $gp, 0($src) !gpdisp!N
        // The GPDISP relocation on the ldah must carry r_addend = distance to
        // the paired lda.  Create a forward symbol for the lda position and a
        // symbol to be defined at the ldah position (emitted before the ldah in
        // matchAndEmitInstruction).  The fixup expression (lda_sym - ldah_sym)
        // resolves to the byte distance after layout.
        MCSymbol *LdaSym = getContext().createTempSymbol("gpdisp_lda");
        MCSymbol *LdahSym = getContext().createTempSymbol("gpdisp_ldah");
        GpDispLdaLabels[GpDispSeq] = LdaSym;
        PendingPreInsnLabels.push_back(LdahSym);
        const MCExpr *Diff = MCBinaryExpr::createSub(
            MCSymbolRefExpr::create(LdaSym, getContext()),
            MCSymbolRefExpr::create(LdahSym, getContext()), getContext());
        static_cast<AlphaOperand &>(*Operands.back())
            .applySpecifierWithExpr(Spec, Diff, getContext());
      } else if (It->second) {
        // Second occurrence: lda $gp, 0($gp) !gpdisp!N
        // Define the forward symbol at this (lda) position and suppress the
        // relocation -- the linker only needs the one on the ldah.
        PendingPreInsnLabels.push_back(It->second);
        // The pair is closed.  Keeping the emptied entry rather than erasing it
        // is what makes a third use of the number an error instead of the
        // silent start of a new pair.
        It->second = nullptr;
      } else {
        return Error(getLexer().getLoc(),
                     "!gpdisp!" + Twine(GpDispSeq) +
                         " is already paired; a sequence number names one "
                         "ldah/lda pair");
      }
    } else if (!static_cast<AlphaOperand &>(*Operands.back())
                    .applySpecifier(Spec, getContext())) {
      // Nothing to write the relocation into.  GNU as calls this "invalid
      // relocation for field"; saying nothing and dropping it leaves the
      // caller with an object missing the relocation they asked for.
      return Error(SpecLoc, "invalid relocation for field");
    }
    PendingSpecifier = Spec;
    PendingSpecifierLoc = SpecLoc;
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
  // A 16-bit value is a single lda; a value that fits 32 bits is an ldah/lda
  // pair (with a zapnot to clear the sign extension of an unsigned 32-bit value
  // whose bit 31 is set).  Anything wider is built from both halves, which is
  // what buildConstantSteps does on its own.
  if (SignExtend64<16>(V) == V) {
    emitAtRaw(Out, L, Alpha::LDAi,
              {MCOperand::createReg(Rc), MCOperand::createImm(V)});
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
    emitAtRaw(Out, L, Alpha::ZAPNOTi,
              {MCOperand::createReg(Rc), MCOperand::createReg(Rc),
               MCOperand::createImm(0xf)});
}

bool AlphaAsmParser::matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  // Emit any symbols that were stashed by parseInstruction to be defined just
  // before this instruction.  Used by !gpdisp!N pair handling to mark the ldah
  // and lda positions so the GPDISP fixup addend (lda_addr - ldah_addr) can
  // be resolved at layout time.
  for (MCSymbol *Sym : PendingPreInsnLabels)
    Out.emitLabel(Sym);
  PendingPreInsnLabels.clear();

  StringRef Mnemonic = op(Operands, 0).getToken();

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
    MCRegister Dst = op(Operands, 1).getReg();
    MCRegister Base = op(Operands, 2).getMemBase();
    const MCExpr *Off = op(Operands, 2).getMemOff();
    const MCExpr *GpDisp =
        MCSpecifierExpr::create(MCConstantExpr::create(4, getContext()),
                                Alpha::fixup_alpha_gpdisp, getContext());
    emitAt(Out, IDLoc, Alpha::LDAHm,
           {MCOperand::createReg(Dst), MCOperand::createReg(Base),
            MCOperand::createExpr(GpDisp)});
    const auto *CE = dyn_cast<MCConstantExpr>(Off);
    emitAt(Out, IDLoc, Alpha::LEA,
           {MCOperand::createReg(Dst), MCOperand::createReg(Dst),
            CE ? MCOperand::createImm(CE->getValue())
               : MCOperand::createExpr(Off)});
    return false;
  }

  // jsr $Ra, ($Rb): emit the bare jsr word.  The operand kinds are checked
  // too, so that a line whose operands are not what the form expects does not
  // read the wrong member of the operand union.
  if (Mnemonic == "jsr" && Operands.size() == 3 && Operands[1]->isReg() &&
      op(Operands, 2).isParenReg()) {
    emitAt(Out, IDLoc, Alpha::JSRr,
           {MCOperand::createReg(op(Operands, 1).getReg()),
            MCOperand::createReg(op(Operands, 2).getMemBase())});
    return false;
  }

  // jsr $Ra, symbol: an indirect call to a symbol reached through the GOT (GNU
  // as's jsr-to-symbol macro).  Load the target's address from its GOT entry
  // into $27 (the procedure value) and jsr through it.
  if (Mnemonic == "jsr" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm() && !isa<MCConstantExpr>(op(Operands, 2).getImm())) {
    MCRegister Ra = op(Operands, 1).getReg();
    const MCExpr *Sym = op(Operands, 2).getImm();
    // ldq $27, symbol($gp) !literal
    emitAtRaw(Out, IDLoc, Alpha::LDQl,
              {MCOperand::createReg(Alpha::R27), MCOperand::createExpr(Sym)});
    // jsr $Ra, ($27)
    emitAt(Out, IDLoc, Alpha::JSRr,
           {MCOperand::createReg(Ra), MCOperand::createReg(Alpha::R27)});
    return false;
  }

  // jmp $Ra, symbol: a jump to a symbol reached through the GOT, expanded like
  // jsr but discarding the return address.
  if (Mnemonic == "jmp" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm() && !isa<MCConstantExpr>(op(Operands, 2).getImm())) {
    const MCExpr *Sym = op(Operands, 2).getImm();
    // ldq $27, symbol($gp) !literal
    emitAtRaw(Out, IDLoc, Alpha::LDQl,
              {MCOperand::createReg(Alpha::R27), MCOperand::createExpr(Sym)});
    // jmp $31, ($27)
    emitAt(Out, IDLoc, Alpha::JMP, {MCOperand::createReg(Alpha::R27)});
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
    const MCExpr *Sym = op(Operands, 2).getImm();
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
      MCRegister R = op(Operands, 1).getReg();
      // ldq $R, symbol($gp) !literal  (the GOT slot is a quadword)
      emitAtRaw(Out, IDLoc, Alpha::LDQl,
                {MCOperand::createReg(R), MCOperand::createExpr(Sym)});
      // <load> $R, 0($R)  (the load itself, of the given width)
      emitAt(Out, IDLoc, DerefOp,
             {MCOperand::createReg(R), MCOperand::createReg(R),
              MCOperand::createImm(0)});
      return false;
    }
  }

  // lda $R, symbol: the address of a symbol is its GOT entry, so GNU as loads
  // it directly (like the ldq form but without the dereference).  A base
  // register (lda $R, disp($base)) or a constant makes this an ordinary lda
  // instead.
  if (Mnemonic == "lda" && Operands.size() == 3 && Operands[1]->isReg() &&
      Operands[2]->isImm()) {
    const MCExpr *Sym = op(Operands, 2).getImm();
    if (!isa<MCConstantExpr>(Sym)) {
      // ldq $R, symbol($gp) !literal
      emitAtRaw(Out, IDLoc, Alpha::LDQl,
                {MCOperand::createReg(op(Operands, 1).getReg()),
                 MCOperand::createExpr(Sym)});
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
        HasBase ? op(Operands, 2).getMemOff() : op(Operands, 2).getImm();
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
      MCRegister Rc = op(Operands, 1).getReg();
      MCRegister Rb = HasBase ? op(Operands, 2).getMemBase() : Alpha::R31;
      if (Rc == Rb)
        return Error(IDLoc, "lda of this displacement needs a scratch register "
                            "distinct from the base");
      if (BigConst) {
        emitLoadImm(Rc, CE->getValue(), IDLoc, Out);
      } else {
        // ldq $Rc, symbol($gp) !literal
        emitAtRaw(Out, IDLoc, Alpha::LDQl,
                  {MCOperand::createReg(Rc), MCOperand::createExpr(Sub)});
        if (Addend) {
          if (!isInt<16>(Addend))
            return Error(IDLoc, "lda addend does not fit a 16-bit "
                                "displacement");
          // lda $Rc, Addend($Rc)
          emitAt(Out, IDLoc, Alpha::LDA,
                 {MCOperand::createReg(Rc), MCOperand::createImm(Addend),
                  MCOperand::createReg(Rc)});
        }
      }
      // $31 reads as zero, so adding it changes nothing; GNU as leaves the add
      // out rather than emitting a no-op.
      if (Rb != Alpha::R31) {
        // addq $Rc, $Rb, $Rc
        emitAt(Out, IDLoc, Alpha::ADDQ,
               {MCOperand::createReg(Rc), MCOperand::createReg(Rc),
                MCOperand::createReg(Rb)});
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
    const MCExpr *E = op(Operands, 1).getImm();
    if (auto *CE = dyn_cast<MCConstantExpr>(E)) {
      int64_t V = CE->getValue();
      if (!isUInt<8>(V))
        return Error(Operands[1]->getStartLoc(),
                     "operand out of range (" + Twine(V) +
                         " is not between 0 and 255)");
      emitAt(Out, IDLoc, Alpha::BISi,
             {MCOperand::createReg(op(Operands, 2).getReg()),
              MCOperand::createReg(Alpha::R31), MCOperand::createImm(V)});
      return false;
    }
  }

  // ldi/ldiq $Rc, imm: load an immediate constant, materializing it in code.
  if ((Mnemonic == "ldi" || Mnemonic == "ldiq") && Operands.size() == 3 &&
      Operands[1]->isReg() && Operands[2]->isImm()) {
    const MCExpr *E = op(Operands, 2).getImm();
    if (auto *CE = dyn_cast<MCConstantExpr>(E)) {
      emitLoadImm(op(Operands, 1).getReg(), CE->getValue(), IDLoc, Out);
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
  if (Result == Match_Success && PendingSpecifier) {
    // A relocation has to fit the field it is written into, which is not known
    // until the encoding is.  GNU as checks the same thing and reports it the
    // same way: a !literal on an operate instruction has nowhere to go, and a
    // 16-bit !gprelhigh does not fit the 14-bit hint field of a ret.
    unsigned Have = Alpha::getRelocField(MII.get(Inst.getOpcode()).TSFlags);
    unsigned Want = specifierRelocField(PendingSpecifier);
    if (Have != Want)
      return Error(PendingSpecifierLoc, "invalid relocation for field");
  }
  switch (Result) {
  case Match_Success:
    Inst.setLoc(IDLoc);
    // The jsr/jmp branch-prediction hint is spelled as a byte offset that the
    // encoder divides by four, so a value with either low bit set does not
    // survive the encoding: `jsr $26, ($27), 1' assembles as a hint of zero.
    // GNU as writes it the same way and says so ("jump hint unaligned"), which
    // is the one thing this used to leave out.  A symbol needs no check -- the
    // hint comes from an R_ALPHA_HINT then, and the linker computes the field
    // itself.
    if (Inst.getOpcode() == Alpha::JSRt || Inst.getOpcode() == Alpha::JMPt) {
      const MCOperand &Hint = Inst.getOperand(2);
      if (Hint.isImm() && (Hint.getImm() & 3))
        Warning(Operands.back()->getStartLoc(),
                "branch-prediction hint is not a multiple of four; its low "
                "bits are dropped");
    }
    if (PendingLituse)
      Inst.setFlags(Inst.getFlags() | Alpha::encodeLituse(PendingLituse - 1));
    if (PendingSeq)
      Inst.setFlags(Inst.getFlags() |
                    Alpha::encodeSeq(PendingSeq, PendingSeqIsLiteral));
    // Whatever was written is what this instruction carries -- including
    // nothing, which is a qualifier too.  Recording it stops -mieee from
    // turning a written `addt' into `addt/su'.  A mnemonic spelled with its
    // qualifier, such as cvttq/svid, already has it in the encoding and needs
    // no flag; it has TrapClass 0 for that reason.
    if (MII.get(Inst.getOpcode()).TSFlags & Alpha::TrapClassMask)
      Inst.setFlags(Flags ? Flags
                          : Alpha::encodeFPQual(0, Alpha::FPRoundNormal));
    emitInst(Inst, Out);
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
