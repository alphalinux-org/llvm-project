//===-- AlphaInstrInfo.td - Alpha instruction definitions --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Alpha instruction set.
//
//===----------------------------------------------------------------------===//

include "AlphaInstrFormats.td"

//===----------------------------------------------------------------------===//
// Call-sequence markers.
//===----------------------------------------------------------------------===//

def SDT_AlphaCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i64>,
                                            SDTCisVT<1, i64>]>;
def SDT_AlphaCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i64>,
                                          SDTCisVT<1, i64>]>;

def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_AlphaCallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_AlphaCallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

let Defs = [R30], Uses = [R30] in {
def ADJCALLSTACKDOWN : AlphaPseudo<(outs), (ins i64imm:$amt1, i64imm:$amt2),
                                   "#ADJCALLSTACKDOWN",
                                   [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : AlphaPseudo<(outs), (ins i64imm:$amt1, i64imm:$amt2),
                                   "#ADJCALLSTACKUP",
                                   [(callseq_end timm:$amt1, timm:$amt2)]>;
}

//===----------------------------------------------------------------------===//
// Target-specific DAG nodes.
//===----------------------------------------------------------------------===//

def AlphaRet : SDNode<"AlphaISD::RET_GLUE", SDTNone,
                      [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// Load the address of a global from the GOT (large-data model): the operand is
// a GP-relative GOT slot resolved by an R_ALPHA_LITERAL relocation.
def AlphaLiteral : SDNode<"AlphaISD::LITERAL", SDTIntUnaryOp>;

// A call: the callee address is in $27 (pv); $26 receives the return address.
def AlphaCall : SDNode<"AlphaISD::CALL", SDTNone,
                       [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                        SDNPVariadic]>;

// A call to a division millicode routine (__divq and friends): operands in
// $24/$25, result in $27, entered through $23.  It clobbers only a few
// registers, so it is modeled separately from a normal call.
def AlphaDivCall : SDNode<"AlphaISD::DIVCALL", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                           SDNPVariadic]>;

// GP-relative address materialization (small/large-data local symbols such as
// constant-pool entries): the high 16 bits via ldah !gprelhigh and the low 16
// bits via lda !gprellow.
def AlphaGprelHi : SDNode<"AlphaISD::GPREL_HI", SDTIntBinOp>;
def AlphaGprelLo : SDNode<"AlphaISD::GPREL_LO", SDTIntBinOp>;

//===----------------------------------------------------------------------===//
// Operands and immediate predicates.
//===----------------------------------------------------------------------===//

// A 16-bit signed displacement, as used by the memory-format instructions.
def s16imm : Operand<i64>;

// An immediate that fits in the 16-bit signed displacement of `lda`.
def immSExt16 : ImmLeaf<i64, [{ return isInt<16>(Imm); }]>;

// An 8-bit unsigned literal, as used by operate instructions in literal form.
def u8imm : Operand<i64>;

// An immediate that fits in the 8-bit operate literal field.
def immUExt8 : ImmLeaf<i64, [{ return isUInt<8>(Imm); }]>;

// A memory address: base register plus 16-bit signed displacement, printed as
// `disp($base)`.
def MemAsmOperand : AsmOperandClass {
  let Name = "Mem";
  let ParserMethod = "parseMemOperand";
}
def memri : Operand<i64> {
  let MIOperandInfo = (ops GPRC, s16imm);
  let PrintMethod = "printMemOperand";
  let EncoderMethod = "getMemEncoding";
  let DecoderMethod = "decodeMemri";
  let ParserMatchClass = MemAsmOperand;
}

// Symbol operands whose 16-bit displacement is filled by a relocation.  The
// operand type selects the relocation: literal (GOT, R_ALPHA_LITERAL), gprel
// high and gprel low (R_ALPHA_GPRELHIGH / R_ALPHA_GPRELLOW).
class SymOperand<string enc> : Operand<i64> {
  let EncoderMethod = enc;
  let DecoderMethod = "decodeImm";
}
def literal_sym : SymOperand<"getLiteralEncoding">;
def gprelhigh_sym : SymOperand<"getGprelHighEncoding">;
def gprellow_sym : SymOperand<"getGprelLowEncoding">;
// Retained alias so existing definitions read naturally.
def symbol : SymOperand<"getGprelLowEncoding">;

// A register printed inside parentheses, `($reg)`, as used by the jsr/jmp/ldq_u
// address forms.  Written this way (rather than literal parens in the asm
// string) so the instructions stay in the assembler matcher and disassembler.
def parenGPR : RegisterOperand<GPRC> {
  let PrintMethod = "printParenReg";
  let DecoderMethod = "DecodeGPRCRegisterClass";
}

// The base register of ldq_u/stq_u, printed `0($reg)`: unlike jsr/jmp, the
// assembler requires an explicit displacement on these memory forms.
def memzGPR : RegisterOperand<GPRC> {
  let PrintMethod = "printZeroDispReg";
  let DecoderMethod = "DecodeGPRCRegisterClass";
}

def ADDRri : ComplexPattern<i64, 2, "SelectADDRri", [frameindex]>;

// Match any address as a single register (used by the pre-BWX byte/word access
// sequences, which need the effective address for both ldq_u and the extract).
def ADDRr : ComplexPattern<i64, 1, "SelectAddrReg", []>;

//===----------------------------------------------------------------------===//
// Instructions.
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

// NOP is the canonical `bis $31, $31, $31`.
def NOP : OForm<0x11, 0x20, (outs), (ins), "nop", []> {
  let Ra = 31;
  let Rb = 31;
  let Rc = 31;
}

// Memory barrier: misc-format instruction with function code 0x4000.
let hasSideEffects = 1 in
def MB : AlphaInst<(outs), (ins), "mb", [(atomic_fence timm, timm)]> {
  let Opcode = 0x18;
  let Inst{25-0} = 0x4000;
}

// Register-register integer operate instruction: `<mnem> $Ra, $Rb, $Rc`
// computes $Rc = $Ra <op> $Rb.
class ALU_rr<bits<6> opc, bits<7> func, string mnem, SDPatternOperator node>
    : OForm<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc",
            [(set i64:$Rc, (node i64:$Ra, i64:$Rb))]>;

def ADDQ  : ALU_rr<0x10, 0x20, "addq",  add>;
def SUBQ  : ALU_rr<0x10, 0x29, "subq",  sub>;
def MULQ  : ALU_rr<0x13, 0x20, "mulq",  mul>;
def UMULH : ALU_rr<0x13, 0x30, "umulh", mulhu>;
def AND   : ALU_rr<0x11, 0x00, "and",   and>;
def XOR   : ALU_rr<0x11, 0x40, "xor",   xor>;

// Scaled add/subtract: `s4addq $Ra, $Rb, $Rc` computes $Rc = $Ra*4 + $Rb (s8
// scales by 8); useful for array-index address arithmetic.
let hasSideEffects = 0 in {
def S4ADDQ : OForm<0x10, 0x22, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s4addq $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (add (shl i64:$Ra, (i64 2)), i64:$Rb))]>;
def S8ADDQ : OForm<0x10, 0x32, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s8addq $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (add (shl i64:$Ra, (i64 3)), i64:$Rb))]>;
def S4SUBQ : OForm<0x10, 0x2b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s4subq $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sub (shl i64:$Ra, (i64 2)), i64:$Rb))]>;
def S8SUBQ : OForm<0x10, 0x3b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s8subq $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sub (shl i64:$Ra, (i64 3)), i64:$Rb))]>;
}

// Literal (8-bit immediate) form of an operate instruction:
// `<mnem> $Ra, lit, $Rc` computes $Rc = $Ra <op> zext(lit).
class ALU_ri<bits<6> opc, bits<7> func, string mnem, SDPatternOperator node>
    : OFormL<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc",
             [(set i64:$Rc, (node i64:$Ra, immUExt8:$lit))]>;

def ADDQi : ALU_ri<0x10, 0x20, "addq", add>;
def SUBQi : ALU_ri<0x10, 0x29, "subq", sub>;
def MULQi : ALU_ri<0x13, 0x20, "mulq", mul>;
def ANDi  : ALU_ri<0x11, 0x00, "and",  and>;
def BISi  : ALU_ri<0x11, 0x20, "bis",  or>;
def XORi  : ALU_ri<0x11, 0x40, "xor",  xor>;

// Shifts use the low 6 bits of $Rb as the shift amount.
def SLL : ALU_rr<0x12, 0x39, "sll", shl>;
def SRL : ALU_rr<0x12, 0x34, "srl", srl>;
def SRA : ALU_rr<0x12, 0x3c, "sra", sra>;

// Immediate shift amount.
def SLLi : ALU_ri<0x12, 0x39, "sll", shl>;
def SRLi : ALU_ri<0x12, 0x34, "srl", srl>;
def SRAi : ALU_ri<0x12, 0x3c, "sra", sra>;

// `zapnot $Ra, lit, $Rc` keeps the bytes of $Ra selected by the 8-bit mask and
// zeroes the rest.  It provides byte-granular zero-extension.
def ZAPNOTi : OFormL<0x12, 0x31, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "zapnot $Ra, $lit, $Rc", []>;

// Byte/word sign-extend from the BWX extension: one instruction in place of the
// sll/sra pair.  The single source is $Rb (with $Ra = $31).
let Predicates = [HasBWX], hasSideEffects = 0, Ra = 31 in {
def SEXTB : OForm<0x1c, 0x00, (outs GPRC:$Rc), (ins GPRC:$Rb), "sextb $Rb, $Rc",
                  [(set i64:$Rc, (sext_inreg i64:$Rb, i8))]>;
def SEXTW : OForm<0x1c, 0x01, (outs GPRC:$Rc), (ins GPRC:$Rb), "sextw $Rb, $Rc",
                  [(set i64:$Rc, (sext_inreg i64:$Rb, i16))]>;
}

// Count instructions from the CIX extension (ev67).  The single source is $Rb
// (with $Ra = $31); ctlz/cttz of zero produce 64, matching ISD semantics.
let Predicates = [HasCIX], hasSideEffects = 0, Ra = 31 in {
def CTPOP : OForm<0x1c, 0x30, (outs GPRC:$Rc), (ins GPRC:$Rb), "ctpop $Rb, $Rc",
                  [(set i64:$Rc, (ctpop i64:$Rb))]>;
def CTLZ : OForm<0x1c, 0x32, (outs GPRC:$Rc), (ins GPRC:$Rb), "ctlz $Rb, $Rc",
                 [(set i64:$Rc, (ctlz i64:$Rb))]>;
def CTTZ : OForm<0x1c, 0x33, (outs GPRC:$Rc), (ins GPRC:$Rb), "cttz $Rb, $Rc",
                 [(set i64:$Rc, (cttz i64:$Rb))]>;
}

// `addl $Ra, $Rb, $Rc` is a 32-bit add whose result is sign-extended to 64
// bits.  With $Rb = $31 it sign-extends the low 32 bits of $Ra.
def ADDL : OForm<0x10, 0x00, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                 "addl $Ra, $Rb, $Rc", []>;

// Comparisons write 0 or 1 into $Rc (Alpha has no condition-code register).
class CMP_rr<bits<7> func, string mnem>
    : OForm<0x10, func, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc", []>;
def CMPEQ  : CMP_rr<0x2d, "cmpeq">;
def CMPLT  : CMP_rr<0x4d, "cmplt">;
def CMPLE  : CMP_rr<0x6d, "cmple">;
def CMPULT : CMP_rr<0x1d, "cmpult">;
def CMPULE : CMP_rr<0x3d, "cmpule">;

// Conditional move: `cmovne $Ra, $Rb, $Rc` sets $Rc = $Rb when $Ra != 0,
// leaving $Rc unchanged otherwise.  With $Rc preset to the false value this
// implements `select`.
let Constraints = "$Rfalse = $Rc" in
def CMOVNE : OForm<0x11, 0x26, (outs GPRC:$Rc),
                   (ins GPRC:$Rfalse, GPRC:$Ra, GPRC:$Rb),
                   "cmovne $Ra, $Rb, $Rc",
                   [(set i64:$Rc,
                         (select i64:$Ra, i64:$Rb, i64:$Rfalse))]>;

// Conditional moves testing $Ra against zero with a specific condition, which
// fold a compare-against-zero into the select instead of a separate cmpXX.
class CMOV_cc<bits<7> func, string mnem, CondCode cc>
    : OForm<0x11, func, (outs GPRC:$Rc), (ins GPRC:$Rfalse, GPRC:$Ra, GPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc",
            [(set i64:$Rc, (select (i64 (setcc i64:$Ra, 0, cc)),
                                   i64:$Rb, i64:$Rfalse))]> {
  let Constraints = "$Rfalse = $Rc";
}
// Only the conditions that survive DAG canonicalization against zero are given
// patterns (setge/setle against 0 are rewritten to setgt/setlt against -1/1).
def CMOVEQ : CMOV_cc<0x24, "cmoveq", SETEQ>;
def CMOVLT : CMOV_cc<0x44, "cmovlt", SETLT>;
def CMOVGT : CMOV_cc<0x66, "cmovgt", SETGT>;

// `bis $Ra, $Rb, $Rc` computes $Rc = $Ra OR $Rb.  copyPhysReg emits it as
// `bis $31, $Rb, $Rc` to move an integer register.
def BIS : ALU_rr<0x11, 0x20, "bis", or>;

// Logical operations with a complemented second operand.  These fold a `not`
// (xor with -1) into a single instruction.
def BIC   : OForm<0x11, 0x08, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "bic $Ra, $Rb, $Rc",
                  [(set i64:$Rc, (and i64:$Ra, (not i64:$Rb)))]>;
def ORNOT : OForm<0x11, 0x28, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "ornot $Ra, $Rb, $Rc",
                  [(set i64:$Rc, (or i64:$Ra, (not i64:$Rb)))]>;
def EQV   : OForm<0x11, 0x48, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "eqv $Ra, $Rb, $Rc",
                  [(set i64:$Rc, (xor i64:$Ra, (not i64:$Rb)))]>;

// Floating-point copy-sign: `cpys $Ra, $Rb, $Rc` builds the sign of $Ra with
// the magnitude of $Rb.  Used by copyPhysReg as `cpys $Rb, $Rb, $Rc` to move a
// floating-point register.
def CPYS : FForm<0x17, 0x020, (outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
                 "cpys $Ra, $Rb, $Rc", []>;
def CPYSN : FForm<0x17, 0x021, (outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
                  "cpysn $Ra, $Rb, $Rc", []>;

// Sign manipulation, built from cpys/cpysn.  Each is defined per floating-point
// type so the result type is unambiguous.  fabs uses the (positive) sign of
// $f31; fneg uses the negated sign of the source; fcopysign takes the magnitude
// of $Ra and the sign of $Rb.  The encodings are written out because Fa/Fb are
// hardcoded ($f31), duplicated or swapped relative to the operand list.
class FSign<bits<11> func, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<(outs FPRC:$Rc), ins, asmstr, pattern> {
  bits<5> fa;
  bits<5> fb;
  bits<5> Rc;
  let Opcode = 0x17;
  let Inst{25-21} = fa;
  let Inst{20-16} = fb;
  let Inst{15-5} = func;
  let Inst{4-0} = Rc;
}
// fabs: cpys $f31, Rb, Rc.
class FAbs<list<dag> pat> : FSign<0x020, (ins FPRC:$Rb), "cpys $$f31, $Rb, $Rc",
                                  pat> {
  bits<5> Rb;
  let fa = 31;
  let fb = Rb;
}
let isCodeGenOnly = 1 in def FABSS : FAbs<[(set f32:$Rc, (fabs f32:$Rb))]>;
let isCodeGenOnly = 1 in def FABST : FAbs<[(set f64:$Rc, (fabs f64:$Rb))]>;
// fneg: cpysn Rb, Rb, Rc.
class FNeg<list<dag> pat> : FSign<0x021, (ins FPRC:$Rb), "cpysn $Rb, $Rb, $Rc",
                                  pat> {
  bits<5> Rb;
  let fa = Rb;
  let fb = Rb;
}
let isCodeGenOnly = 1 in def FNEGS : FNeg<[(set f32:$Rc, (fneg f32:$Rb))]>;
let isCodeGenOnly = 1 in def FNEGT : FNeg<[(set f64:$Rc, (fneg f64:$Rb))]>;
// fcopysign: cpys Rb, Ra, Rc (sign of Rb, magnitude of Ra).
class FCpys<list<dag> pat>
    : FSign<0x020, (ins FPRC:$Ra, FPRC:$Rb), "cpys $Rb, $Ra, $Rc", pat> {
  bits<5> Ra;
  bits<5> Rb;
  let fa = Rb;
  let fb = Ra;
}
let isCodeGenOnly = 1 in def FCPYSS : FCpys<[(set f32:$Rc, (fcopysign f32:$Ra, f32:$Rb))]>;
let isCodeGenOnly = 1 in def FCPYST : FCpys<[(set f64:$Rc, (fcopysign f64:$Ra, f64:$Rb))]>;

// IEEE floating-point operate instruction: `<mnem> $Ra, $Rb, $Rc`.  The `s`
// forms operate on S_floating (f32), the `t` forms on T_floating (f64).
class FP_rr<bits<11> func, string mnem, ValueType vt, SDPatternOperator node>
    : FForm<0x16, func, (outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc",
            [(set vt:$Rc, (node vt:$Ra, vt:$Rb))]>;

def ADDS : FP_rr<0x080, "adds", f32, fadd>;
def ADDT : FP_rr<0x0a0, "addt", f64, fadd>;
def SUBS : FP_rr<0x081, "subs", f32, fsub>;
def SUBT : FP_rr<0x0a1, "subt", f64, fsub>;
def MULS : FP_rr<0x082, "muls", f32, fmul>;
def MULT : FP_rr<0x0a2, "mult", f64, fmul>;
def DIVS : FP_rr<0x083, "divs", f32, fdiv>;
def DIVT : FP_rr<0x0a3, "divt", f64, fdiv>;

// Floating-point comparisons write 2.0 (true) or 0.0 (false) into an FP
// register.  They operate on the T_floating register format, which both f32 and
// f64 values use, so the same instructions serve both.
class CMPT_rr<bits<11> func, string mnem>
    : FForm<0x16, func, (outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc", []>;
def CMPTEQ : CMPT_rr<0x0a5, "cmpteq">;
def CMPTLT : CMPT_rr<0x0a6, "cmptlt">;
def CMPTLE : CMPT_rr<0x0a7, "cmptle">;

// Floating-point conditional move: `fcmovne $Fa, $Fb, $Fc` sets $Fc = $Fb when
// $Fa != 0.0.  With $Fc preset to the false value this implements an FP select.
let Constraints = "$Ffalse = $Rc" in
def FCMOVNE : FForm<0x17, 0x02b, (outs FPRC:$Rc),
                    (ins FPRC:$Ffalse, FPRC:$Ra, FPRC:$Rb),
                    "fcmovne $Ra, $Rb, $Rc", []>;

// Unary floating-point operate: single source $Rb (with Fa = $f31), result $Rc.
class FP_r<bits<6> opc, bits<11> func, string mnem, list<dag> pat>
    : FForm<opc, func, (outs FPRC:$Rc), (ins FPRC:$Rb), mnem # " $Rb, $Rc", pat> {
  let Ra = 31;
}
// Precision conversions between S_floating (f32) and T_floating (f64).
def CVTST : FP_r<0x16, 0x2ac, "cvtst", [(set f64:$Rc, (fpextend f32:$Rb))]>;
def CVTTS : FP_r<0x16, 0x0ac, "cvtts", [(set f32:$Rc, (fpround f64:$Rb))]>;

// Square root, from the FIX extension (ev6).
let Predicates = [HasFIX], hasSideEffects = 0 in {
def SQRTS : FP_r<0x14, 0x08b, "sqrts", [(set f32:$Rc, (fsqrt f32:$Rb))]>;
def SQRTT : FP_r<0x14, 0x0ab, "sqrtt", [(set f64:$Rc, (fsqrt f64:$Rb))]>;
}

// Integer/floating conversions on values held in floating-point registers.
def CVTQT : FP_r<0x16, 0x0be, "cvtqt", []>;
def CVTQS : FP_r<0x16, 0x0bc, "cvtqs", []>;
// cvttq/c: convert T-floating to quadword with chopped (round-toward-zero)
// rounding, matching C's fp-to-integer truncation semantics.
def CVTTQ : FP_r<0x16, 0x02f, "cvttq/c", []>;

// Load address: `lda $Rc, $disp($Rb)` computes $Rc = $Rb + sign_ext($disp).
// With $Rb = $31 (zero) this materializes a 16-bit signed constant.  The
// split-operand form is used by codegen; LEA/LDAHm below carry the single
// memory operand that the assembler and disassembler parse and print.
let isCodeGenOnly = 1 in {
def LDA : MFormD<0x08, (outs GPRC:$Ra), (ins s16imm:$disp, GPRC:$Rb),
                 "lda $Ra, $disp($Rb)", []>;
def LDAH : MFormD<0x09, (outs GPRC:$Ra), (ins s16imm:$disp, GPRC:$Rb),
                  "ldah $Ra, $disp($Rb)", []>;
}

// lda/ldah with a base+displacement memory operand.  LEA also materializes a
// frame index (rewritten by eliminateFrameIndex).
let hasSideEffects = 0 in {
def LEA : MForm<0x08, (outs GPRC:$Ra), (ins memri:$addr), "lda $Ra, $addr", []>;
def LDAHm : MForm<0x09, (outs GPRC:$Ra), (ins memri:$addr), "ldah $Ra, $addr",
                  []>;
}

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

// Materialize a small signed constant with `lda $Rc, imm($31)`.
def : Pat<(i64 immSExt16:$imm), (LDA immSExt16:$imm, R31)>;

// `not $x` is `ornot $31, $x` (0 OR ~x), avoiding an explicit -1.
def : Pat<(not GPRC:$x), (ORNOT R31, GPRC:$x)>;

//===----------------------------------------------------------------------===//
// Global pointer and global addresses.
//===----------------------------------------------------------------------===//

// Establish the global pointer from the procedure value on entry.  `ldgp` is a
// gas macro that expands to an ldah/lda pair with matching !gpdisp relocations.
let Uses = [R27], Defs = [R29], hasSideEffects = 1, isPseudo = 1, Size = 8 in
def LDGP : AlphaInst<(outs), (ins), "ldgp $$29, 0($$27)", []>;

// Load a global's address from its GOT slot via an R_ALPHA_LITERAL relocation.
let mayLoad = 1, hasSideEffects = 0, Uses = [R29] in
let isCodeGenOnly = 1 in
def LDQl : MFormD<0x29, (outs GPRC:$Ra), (ins literal_sym:$disp),
                  "ldq $Ra, $disp($$29)\t\t!literal", []> {
  let Rb = 29;
}
def : Pat<(AlphaLiteral tglobaladdr:$sym), (LDQl tglobaladdr:$sym)>;
def : Pat<(AlphaLiteral texternalsym:$sym), (LDQl texternalsym:$sym)>;

// GP-relative high/low address parts.  The relocation suffix distinguishes the
// two; the same symbol operand feeds both.
let hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDAHg : MFormD<0x09, (outs GPRC:$Ra), (ins gprelhigh_sym:$disp, GPRC:$Rb),
                   "ldah $Ra, $disp($Rb)\t\t!gprelhigh", []>;
def LDAg : MFormD<0x08, (outs GPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "lda $Ra, $disp($Rb)\t\t!gprellow", []>;
}
def : Pat<(AlphaGprelHi tconstpool:$sym, GPRC:$base),
          (LDAHg tconstpool:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelLo tconstpool:$sym, GPRC:$base),
          (LDAg tconstpool:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelHi tjumptable:$sym, GPRC:$base),
          (LDAHg tjumptable:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelLo tjumptable:$sym, GPRC:$base),
          (LDAg tjumptable:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelHi tglobaladdr:$sym, GPRC:$base),
          (LDAHg tglobaladdr:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelLo tglobaladdr:$sym, GPRC:$base),
          (LDAg tglobaladdr:$sym, GPRC:$base)>;

// A load of a GP-relative address folds the !gprellow low part into the load
// displacement, so only the ldah !gprelhigh is needed to form the address.
let mayLoad = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDLg : MFormD<0x28, (outs GPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "ldl $Ra, $disp($Rb)\t\t!gprellow", []>;
def LDQg : MFormD<0x29, (outs GPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "ldq $Ra, $disp($Rb)\t\t!gprellow", []>;
def LDTg : MFormD<0x23, (outs FPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "ldt $Ra, $disp($Rb)\t\t!gprellow", []>;
def LDSg : MFormD<0x22, (outs FPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "lds $Ra, $disp($Rb)\t\t!gprellow", []>;
}
// The corresponding stores, used for small-data globals.
let mayStore = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def STLg : MFormD<0x2c, (outs), (ins GPRC:$Ra, gprellow_sym:$disp, GPRC:$Rb),
                  "stl $Ra, $disp($Rb)\t\t!gprellow", []>;
def STQg : MFormD<0x2d, (outs), (ins GPRC:$Ra, gprellow_sym:$disp, GPRC:$Rb),
                  "stq $Ra, $disp($Rb)\t\t!gprellow", []>;
def STTg : MFormD<0x27, (outs), (ins FPRC:$Ra, gprellow_sym:$disp, GPRC:$Rb),
                  "stt $Ra, $disp($Rb)\t\t!gprellow", []>;
def STSg : MFormD<0x26, (outs), (ins FPRC:$Ra, gprellow_sym:$disp, GPRC:$Rb),
                  "sts $Ra, $disp($Rb)\t\t!gprellow", []>;
}
// Fold the !gprellow low part into the load/store for constant pools and (with
// small-data) globals.
multiclass GprelFold<SDNode sym> {
def : Pat<(i64 (sextloadi32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDLg sym:$s, GPRC:$b)>;
// A longword load that is not sign-extending needs the same fold: an extending
// load of don't-care high bits is the sign-extending instruction all the same,
// and a zero-extending one is that instruction with the high bits cleared
// afterwards.
def : Pat<(i64 (extloadi32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDLg sym:$s, GPRC:$b)>;
def : Pat<(i64 (zextloadi32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (ZAPNOTi (LDLg sym:$s, GPRC:$b), 15)>;
def : Pat<(i64 (load (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDQg sym:$s, GPRC:$b)>;
def : Pat<(f64 (load (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDTg sym:$s, GPRC:$b)>;
def : Pat<(f32 (load (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDSg sym:$s, GPRC:$b)>;
def : Pat<(f64 (extloadf32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (CVTST (LDSg sym:$s, GPRC:$b))>;
def : Pat<(truncstorei32 GPRC:$v, (AlphaGprelLo sym:$s, GPRC:$b)),
          (STLg GPRC:$v, sym:$s, GPRC:$b)>;
def : Pat<(store GPRC:$v, (AlphaGprelLo sym:$s, GPRC:$b)),
          (STQg GPRC:$v, sym:$s, GPRC:$b)>;
def : Pat<(store f64:$v, (AlphaGprelLo sym:$s, GPRC:$b)),
          (STTg FPRC:$v, sym:$s, GPRC:$b)>;
def : Pat<(store f32:$v, (AlphaGprelLo sym:$s, GPRC:$b)),
          (STSg FPRC:$v, sym:$s, GPRC:$b)>;
}
let AddedComplexity = 20 in {
defm : GprelFold<tconstpool>;
defm : GprelFold<tglobaladdr>;
}

// Call through the procedure value in $27.  jsr leaves the return address in
// $26; the following ldgp reloads the caller's global pointer, which the callee
// may have changed.  $29 is redefined with the caller's gp.
let isCall = 1, Uses = [R27], Defs = [R26, R29], Size = 12,
    isCodeGenOnly = 1 in
def JSR : MbrForm<0b01, (outs), (ins), "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                  [(AlphaCall)]> {
  let Ra = 26;
  let Rb = 27;
}

// A bare jsr word (no ldgp reload); the assembler emits it for `jsr $Ra, ($Rb)`
// and the disassembler decodes a jsr word with no branch-prediction hint to it.
let isCall = 1 in
def JSRr : MbrForm<0b01, (outs), (ins GPRC:$Ra, parenGPR:$Rb),
                   "jsr $Ra, $Rb", []>;

// Division millicode call: enter __divq etc. through $23; the routine preserves
// all but a handful of registers, so no ldgp reload is needed afterwards.
let isCall = 1, Uses = [R24, R25, R27],
    Defs = [R23, R24, R25, R27, R28], isCodeGenOnly = 1 in
def DIVCALL : MbrForm<0b01, (outs), (ins), "jsr $$23, ($$27)", [(AlphaDivCall)]> {
  let Ra = 23;
  let Rb = 27;
}

//===----------------------------------------------------------------------===//
// Integer/floating register moves and conversions.
//===----------------------------------------------------------------------===//

// Alpha has no direct move between integer and floating-point registers, so the
// bit pattern is bounced through an 8-byte stack slot.  A custom inserter
// creates the slot and the stq/ldt (or stt/ldq) pair.
let usesCustomInserter = 1 in {
def MOVi2f : AlphaPseudo<(outs FPRC:$dst), (ins GPRC:$src), "# MOVi2f",
                         [(set f64:$dst, (bitconvert i64:$src))]>;
def MOVf2i : AlphaPseudo<(outs GPRC:$dst), (ins FPRC:$src), "# MOVf2i",
                         [(set i64:$dst, (bitconvert f64:$src))]>;
}

// Signed integer/floating conversions: move the bits into a floating-point
// register, then convert.  f32 results/inputs go through f64.
def : Pat<(f64 (sint_to_fp GPRC:$x)), (CVTQT (MOVi2f GPRC:$x))>;
def : Pat<(f32 (sint_to_fp GPRC:$x)), (CVTQS (MOVi2f GPRC:$x))>;
def : Pat<(i64 (fp_to_sint f64:$x)), (MOVf2i (CVTTQ f64:$x))>;
def : Pat<(i64 (fp_to_sint f32:$x)), (MOVf2i (CVTTQ (CVTST f32:$x)))>;

// Zero-extension of the low 1/2/4 bytes via zapnot (the byte mask keeps the
// low N bytes).  This also lets `and` with these masks avoid materializing a
// wide constant.
def : Pat<(and GPRC:$x, 0xff),       (ZAPNOTi GPRC:$x, 1)>;
def : Pat<(and GPRC:$x, 0xffff),     (ZAPNOTi GPRC:$x, 3)>;
def : Pat<(and GPRC:$x, 0xffffffff), (ZAPNOTi GPRC:$x, 15)>;

// Sign-extend the low 32 bits with `addl $x, $31, $dst`.
def : Pat<(sext_inreg GPRC:$x, i32), (ADDL GPRC:$x, R31)>;

// Sign-extend the low 8/16 bits by shifting the field to the top of the
// register and back with an arithmetic shift.  (BWX has sextb/sextw, which can
// replace these when +bwx is enabled.)
def : Pat<(sext_inreg GPRC:$x, i8),  (SRAi (SLLi GPRC:$x, 56), 56)>;
def : Pat<(sext_inreg GPRC:$x, i16), (SRAi (SLLi GPRC:$x, 48), 48)>;

// setcc.  Conditions without a direct instruction are formed by swapping the
// operands (gt/ge) or inverting equality (ne).
def : Pat<(setcc GPRC:$a, GPRC:$b, SETEQ),  (CMPEQ  GPRC:$a, GPRC:$b)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETNE),  (XORi (CMPEQ GPRC:$a, GPRC:$b), 1)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETLT),  (CMPLT  GPRC:$a, GPRC:$b)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETLE),  (CMPLE  GPRC:$a, GPRC:$b)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETGT),  (CMPLT  GPRC:$b, GPRC:$a)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETGE),  (CMPLE  GPRC:$b, GPRC:$a)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETULT), (CMPULT GPRC:$a, GPRC:$b)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETULE), (CMPULE GPRC:$a, GPRC:$b)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETUGT), (CMPULT GPRC:$b, GPRC:$a)>;
def : Pat<(setcc GPRC:$a, GPRC:$b, SETUGE), (CMPULE GPRC:$b, GPRC:$a)>;

// Ordered floating-point comparisons.  The compare leaves 2.0 or 0.0 in an FP
// register; moving those bits to an integer register and shifting right by 62
// yields 1 or 0.  Greater-than forms swap the operands.
class FCmpPat<CondCode cc, ValueType vt, Instruction cmp, dag a, dag b>
    : Pat<(setcc vt:$x, vt:$y, cc), (SRLi (MOVf2i (cmp a, b)), 62)>;

foreach vt = [f32, f64] in {
def : FCmpPat<SETOEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FCmpPat<SETOLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FCmpPat<SETOLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FCmpPat<SETOGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FCmpPat<SETOGE, vt, CMPTLE, (vt $y), (vt $x)>;
// The NaN-don't-care conditions map to the same ordered instructions.
def : FCmpPat<SETEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FCmpPat<SETLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FCmpPat<SETLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FCmpPat<SETGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FCmpPat<SETGE, vt, CMPTLE, (vt $y), (vt $x)>;
}

// Floating-point select: move the 0/1 condition into an FP register (its bits
// are zero or non-zero) and use fcmovne to pick the true value.
def : Pat<(f32 (select GPRC:$c, FPRC:$t, FPRC:$f)),
          (FCMOVNE FPRC:$f, (MOVi2f GPRC:$c), FPRC:$t)>;
def : Pat<(f64 (select GPRC:$c, FPRC:$t, FPRC:$f)),
          (FCMOVNE FPRC:$f, (MOVi2f GPRC:$c), FPRC:$t)>;

//===----------------------------------------------------------------------===//
// Aligned loads and stores (base + 16-bit displacement).
//===----------------------------------------------------------------------===//

let mayLoad = 1, hasSideEffects = 0 in {
def LDL : MForm<0x28, (outs GPRC:$Ra), (ins memri:$addr), "ldl $Ra, $addr",
                [(set i64:$Ra, (sextloadi32 ADDRri:$addr))]>;
def LDQ : MForm<0x29, (outs GPRC:$Ra), (ins memri:$addr), "ldq $Ra, $addr",
                [(set i64:$Ra, (load ADDRri:$addr))]>;
def LDT : MForm<0x23, (outs FPRC:$Ra), (ins memri:$addr), "ldt $Ra, $addr",
                [(set f64:$Ra, (load ADDRri:$addr))]>;
def LDS : MForm<0x22, (outs FPRC:$Ra), (ins memri:$addr), "lds $Ra, $addr",
                [(set f32:$Ra, (load ADDRri:$addr))]>;
}

// An i32 load with don't-care high bits can use the sign-extending ldl; a
// zero-extending load clears the high half with zapnot.
def : Pat<(i64 (extloadi32 ADDRri:$addr)), (LDL memri:$addr)>;
def : Pat<(i64 (zextloadi32 ADDRri:$addr)), (ZAPNOTi (LDL memri:$addr), 15)>;

// An f32 loaded and widened to f64 (e.g. an f32-encoded double constant): lds
// loads the S_floating value and cvtst widens it exactly.
def : Pat<(f64 (extloadf32 ADDRri:$addr)), (CVTST (LDS memri:$addr))>;

// Byte and word memory access (BWX extension, ev56 and later).  ldbu/ldwu
// zero-extend; stb/stw store the low 8/16 bits.  Signed byte/word loads are
// expanded to an extending load plus sign-extension (see setLoadExtAction).
let Predicates = [HasBWX] in {
let mayLoad = 1, hasSideEffects = 0 in {
def LDBU : MForm<0x0a, (outs GPRC:$Ra), (ins memri:$addr), "ldbu $Ra, $addr",
                 [(set i64:$Ra, (zextloadi8 ADDRri:$addr))]>;
def LDWU : MForm<0x0c, (outs GPRC:$Ra), (ins memri:$addr), "ldwu $Ra, $addr",
                 [(set i64:$Ra, (zextloadi16 ADDRri:$addr))]>;
}
let mayStore = 1, hasSideEffects = 0 in {
def STB : MForm<0x0e, (outs), (ins GPRC:$Ra, memri:$addr), "stb $Ra, $addr",
                [(truncstorei8 i64:$Ra, ADDRri:$addr)]>;
def STW : MForm<0x0d, (outs), (ins GPRC:$Ra, memri:$addr), "stw $Ra, $addr",
                [(truncstorei16 i64:$Ra, ADDRri:$addr)]>;
}
def : Pat<(i64 (extloadi8 ADDRri:$addr)), (LDBU memri:$addr)>;
def : Pat<(i64 (extloadi16 ADDRri:$addr)), (LDWU memri:$addr)>;
} // Predicates = [HasBWX]

// Pre-BWX byte/word access.  ldq_u loads the aligned quadword containing the
// datum (ignoring the low three address bits); extbl/extwl extract the byte or
// word at the position given by the low address bits, zero-extended.
// ldq_u/stq_u take a base register with an implicit zero displacement; the CPU
// ignores the low three address bits.
let mayLoad = 1, hasSideEffects = 0 in
def LDQ_U : MFormZ<0x0b, (outs GPRC:$Ra), (ins memzGPR:$base),
                   "ldq_u $Ra, $base", []>;
let hasSideEffects = 0 in {
def EXTBL : OForm<0x12, 0x06, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "extbl $Ra, $Rb, $Rc", []>;
def EXTWL : OForm<0x12, 0x16, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "extwl $Ra, $Rb, $Rc", []>;
}
let Predicates = [NotBWX] in {
def : Pat<(i64 (zextloadi8 ADDRr:$a)),  (EXTBL (LDQ_U GPRC:$a), GPRC:$a)>;
def : Pat<(i64 (extloadi8 ADDRr:$a)),   (EXTBL (LDQ_U GPRC:$a), GPRC:$a)>;
def : Pat<(i64 (zextloadi16 ADDRr:$a)), (EXTWL (LDQ_U GPRC:$a), GPRC:$a)>;
def : Pat<(i64 (extloadi16 ADDRr:$a)),  (EXTWL (LDQ_U GPRC:$a), GPRC:$a)>;
}

// Pre-BWX byte/word store: read the aligned quadword (ldq_u), clear the target
// field (mskbl/mskwl), splice in the positioned datum (insbl/inswl) and write
// the quadword back (stq_u).  This read-modify-write is not atomic with respect
// to other accesses in the same quadword (see -msafe-bwa); a lock-based variant
// is a later addition.
let hasSideEffects = 0 in {
def INSBL : OForm<0x12, 0x0b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "insbl $Ra, $Rb, $Rc", []>;
def INSWL : OForm<0x12, 0x1b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "inswl $Ra, $Rb, $Rc", []>;
def MSKBL : OForm<0x12, 0x02, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "mskbl $Ra, $Rb, $Rc", []>;
def MSKWL : OForm<0x12, 0x12, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                  "mskwl $Ra, $Rb, $Rc", []>;
}
let mayStore = 1, hasSideEffects = 0 in
def STQ_U : MFormZ<0x0f, (outs), (ins GPRC:$Ra, memzGPR:$base),
                   "stq_u $Ra, $base", []>;
// bic with an 8-bit literal, used to align an address down to a quadword.
let hasSideEffects = 0 in
def BICi : OFormL<0x11, 0x08, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                  "bic $Ra, $lit, $Rc", []>;

// The default (non-atomic) byte/word store read-modify-writes the quadword
// holding the field.  It stays one instruction until after register allocation
// (the two scratch registers are outputs so the allocator provides them):
// stores of different bytes of one quadword are independent in the DAG, so if
// the read and the write were separate instructions another byte's write could
// land between them and be lost.  Selected by AlphaDAGToDAGISel::Select and
// expanded by AlphaInstrInfo::expandPostRAPseudo.
// Size is the expansion's, five instructions, not the one word a pseudo
// defaults to: determineCalleeSaves adds these up before expandPostRAPseudo
// runs, to decide whether the function is close enough to the 21-bit branch
// range to need a global pointer for relaxation.  Counting each of these as one
// word understates a function full of them by a factor of five, which can leave
// a relaxed branch reaching for a $29 the prologue never established.
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, Predicates = [UnsafeBWStore],
    Size = 20, Constraints = "@earlyclobber $t1,@earlyclobber $t2" in {
def RMW_STOREI8 : AlphaPseudo<(outs GPRC:$t1, GPRC:$t2),
                              (ins GPRC:$val, GPRC:$addr), "", []>;
def RMW_STOREI16 : AlphaPseudo<(outs GPRC:$t1, GPRC:$t2),
                               (ins GPRC:$val, GPRC:$addr), "", []>;
}

// Lock-based byte/word store (-msafe-bwa): the read-modify-write is done with
// an ldq_l/stq_c loop by a custom inserter.
let usesCustomInserter = 1, mayLoad = 1, mayStore = 1,
    Predicates = [SafeBWStore] in {
def SAFE_STOREI8  : AlphaPseudo<(outs), (ins GPRC:$val, GPRC:$addr), "",
                                [(truncstorei8 GPRC:$val, ADDRr:$addr)]>;
def SAFE_STOREI16 : AlphaPseudo<(outs), (ins GPRC:$val, GPRC:$addr), "",
                                [(truncstorei16 GPRC:$val, ADDRr:$addr)]>;
}

let mayStore = 1, hasSideEffects = 0 in {
def STL : MForm<0x2c, (outs), (ins GPRC:$Ra, memri:$addr), "stl $Ra, $addr",
                [(truncstorei32 i64:$Ra, ADDRri:$addr)]>;
def STQ : MForm<0x2d, (outs), (ins GPRC:$Ra, memri:$addr), "stq $Ra, $addr",
                [(store i64:$Ra, ADDRri:$addr)]>;
def STT : MForm<0x27, (outs), (ins FPRC:$Ra, memri:$addr), "stt $Ra, $addr",
                [(store f64:$Ra, ADDRri:$addr)]>;
def STS : MForm<0x26, (outs), (ins FPRC:$Ra, memri:$addr), "sts $Ra, $addr",
                [(store f32:$Ra, ADDRri:$addr)]>;
}

// Naturally aligned integer loads and stores are atomic; stronger orderings get
// an explicit memory barrier inserted around them (shouldInsertFencesForAtomic).
def : Pat<(i64 (atomic_load_64 ADDRri:$addr)), (LDQ memri:$addr)>;
// ldl sign-extends, which is Alpha's canonical form for a 32-bit value, so it
// serves the sign-extending load and the one whose high half is don't-care.  A
// zero-extending atomic i32 load -- DAGCombiner::tryToFoldExtOfAtomicLoad forms
// one out of `zext (atomic load i32)' -- has to have the high half cleared.
def : Pat<(i64 (atomic_load_asext_32 ADDRri:$addr)), (LDL memri:$addr)>;
def : Pat<(i64 (atomic_load_zext_32 ADDRri:$addr)),
          (ZAPNOTi (LDL memri:$addr), 15)>;
def : Pat<(atomic_store_64 GPRC:$val, ADDRri:$addr),
          (STQ GPRC:$val, memri:$addr)>;
def : Pat<(atomic_store_32 GPRC:$val, ADDRri:$addr),
          (STL GPRC:$val, memri:$addr)>;

// Load-locked / store-conditional, the primitives for atomic read-modify-write.
// stq_c overwrites its source register with 1 on success or 0 on failure.
let mayLoad = 1, hasSideEffects = 0 in {
def LDL_L : MForm<0x2a, (outs GPRC:$Ra), (ins memri:$addr), "ldl_l $Ra, $addr",
                  []>;
def LDQ_L : MForm<0x2b, (outs GPRC:$Ra), (ins memri:$addr), "ldq_l $Ra, $addr",
                  []>;
}
let mayStore = 1, Constraints = "$Ra = $out", hasSideEffects = 0 in {
def STL_C : MForm<0x2e, (outs GPRC:$out), (ins GPRC:$Ra, memri:$addr),
                  "stl_c $Ra, $addr", []>;
def STQ_C : MForm<0x2f, (outs GPRC:$out), (ins GPRC:$Ra, memri:$addr),
                  "stq_c $Ra, $addr", []>;
}

// Atomic read-modify-write pattern fragments by memory width.
class AtomicI8<SDPatternOperator op>
    : PatFrag<(ops node:$ptr, node:$val), (op node:$ptr, node:$val)> {
  let IsAtomic = 1;
  let MemoryVT = i8;
}
class AtomicI16<SDPatternOperator op>
    : PatFrag<(ops node:$ptr, node:$val), (op node:$ptr, node:$val)> {
  let IsAtomic = 1;
  let MemoryVT = i16;
}
class AtomicI32<SDPatternOperator op>
    : PatFrag<(ops node:$ptr, node:$val), (op node:$ptr, node:$val)> {
  let IsAtomic = 1;
  let MemoryVT = i32;
}

// Atomic read-modify-write pseudos, expanded to an ldl_l/ldq_l retry loop by a
// custom inserter.  $dst receives the value read before the operation.
class AtomicRMW<PatFrag node>
    : AlphaPseudo<(outs GPRC:$dst), (ins GPRC:$addr, GPRC:$val), "",
                  [(set i64:$dst, (node iPTR:$addr, i64:$val))]> {
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let mayStore = 1;
}

// 64-bit memory operand.
class AtomicI64<SDPatternOperator op>
    : PatFrag<(ops node:$ptr, node:$val), (op node:$ptr, node:$val)> {
  let IsAtomic = 1;
  let MemoryVT = i64;
}

def ATOMIC_ADD_I64  : AtomicRMW<AtomicI64<atomic_load_add>>;
def ATOMIC_SUB_I64  : AtomicRMW<AtomicI64<atomic_load_sub>>;
def ATOMIC_AND_I64  : AtomicRMW<AtomicI64<atomic_load_and>>;
def ATOMIC_OR_I64   : AtomicRMW<AtomicI64<atomic_load_or>>;
def ATOMIC_XOR_I64  : AtomicRMW<AtomicI64<atomic_load_xor>>;
def ATOMIC_XCHG_I64 : AtomicRMW<AtomicI64<atomic_swap>>;

def ATOMIC_ADD_I32  : AtomicRMW<AtomicI32<atomic_load_add>>;
def ATOMIC_SUB_I32  : AtomicRMW<AtomicI32<atomic_load_sub>>;
def ATOMIC_AND_I32  : AtomicRMW<AtomicI32<atomic_load_and>>;
def ATOMIC_OR_I32   : AtomicRMW<AtomicI32<atomic_load_or>>;
def ATOMIC_XOR_I32  : AtomicRMW<AtomicI32<atomic_load_xor>>;
def ATOMIC_XCHG_I32 : AtomicRMW<AtomicI32<atomic_swap>>;

def ATOMIC_ADD_I16  : AtomicRMW<AtomicI16<atomic_load_add>>;
def ATOMIC_SUB_I16  : AtomicRMW<AtomicI16<atomic_load_sub>>;
def ATOMIC_AND_I16  : AtomicRMW<AtomicI16<atomic_load_and>>;
def ATOMIC_OR_I16   : AtomicRMW<AtomicI16<atomic_load_or>>;
def ATOMIC_XOR_I16  : AtomicRMW<AtomicI16<atomic_load_xor>>;
def ATOMIC_XCHG_I16 : AtomicRMW<AtomicI16<atomic_swap>>;

def ATOMIC_ADD_I8  : AtomicRMW<AtomicI8<atomic_load_add>>;
def ATOMIC_SUB_I8  : AtomicRMW<AtomicI8<atomic_load_sub>>;
def ATOMIC_AND_I8  : AtomicRMW<AtomicI8<atomic_load_and>>;
def ATOMIC_OR_I8   : AtomicRMW<AtomicI8<atomic_load_or>>;
def ATOMIC_XOR_I8  : AtomicRMW<AtomicI8<atomic_load_xor>>;
def ATOMIC_XCHG_I8 : AtomicRMW<AtomicI8<atomic_swap>>;

// Compare-and-swap, also expanded to an ldq_l/stq_c loop.  $dst receives the
// value read; the caller compares it against $cmp to determine success.
//
// Each fragment is constrained to its access width.  An unconstrained
// atomic_cmp_swap would match a cmpxchg of any width and give a narrower one
// the 64-bit ldq_l/stq_c loop -- the wrong access width and the wrong
// comparison.
class AtomicCAS<PatFrag node>
    : AlphaPseudo<(outs GPRC:$dst), (ins GPRC:$addr, GPRC:$cmp, GPRC:$new), "",
                  [(set i64:$dst, (node iPTR:$addr, i64:$cmp, i64:$new))]> {
  let usesCustomInserter = 1;
  let mayLoad = 1;
  let mayStore = 1;
}
class AtomicCASvt<ValueType vt>
    : PatFrag<(ops node:$ptr, node:$cmp, node:$new),
              (atomic_cmp_swap node:$ptr, node:$cmp, node:$new)> {
  let IsAtomic = 1;
  let MemoryVT = vt;
}
def ATOMIC_CMPXCHG_I64 : AtomicCAS<AtomicCASvt<i64>>;
def ATOMIC_CMPXCHG_I16 : AtomicCAS<AtomicCASvt<i16>>;
def ATOMIC_CMPXCHG_I8  : AtomicCAS<AtomicCASvt<i8>>;

// The ldq_l/stq_c loops themselves.  The pseudos above are what the patterns
// match; EmitInstrWithCustomInserter rewrites each into one of these, which
// carries the scratch registers the loop needs as extra outputs so that the
// register allocator, and not the expansion, is what picks them.  The loop is
// built from these by AlphaExpandAtomicPseudo, after allocation and after the
// post-RA scheduler: the Alpha architecture requires that no memory access
// appear between the load locked and the store conditional, and a spill or a
// reload placed there makes the store conditional fail every time round.
//
// Size is the expansion's, not the one word a pseudo defaults to: the same
// reason RMW_STOREI8 gives above, that determineCalleeSaves adds these up
// before the expansion happens.
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, hasNoSchedulingInfo = 1 in {

// ldq_l, the operation, stq_c, beq.  $alu is the operate-format opcode applied
// to (old value, operand), or zero for an exchange; $is32 selects ldl_l/stl_c.
let Size = 16, Constraints = "@earlyclobber $dst,@earlyclobber $t0" in
def ATOMIC_RMW_LOOP : AlphaPseudo<(outs GPRC:$dst, GPRC:$t0),
                                  (ins GPRC:$addr, GPRC:$val, i64imm:$alu,
                                       i64imm:$is32), "", []>;

// ldq_l, sign-extend the comparand for the longword form, cmpeq, beq, bis,
// stq_c, beq.
let Size = 28,
    Constraints = "@earlyclobber $dst,@earlyclobber $t0,@earlyclobber $t1" in
def ATOMIC_CAS_LOOP : AlphaPseudo<(outs GPRC:$dst, GPRC:$t0, GPRC:$t1),
                                  (ins GPRC:$addr, GPRC:$cmp, GPRC:$new,
                                       i64imm:$is32), "", []>;

// bic, then ldq_l, extract, sign-extend, the operation, mask, insert, merge,
// stq_c and beq.  $isword selects the word forms over the byte forms.
let Size = 44,
    Constraints = "@earlyclobber $dst,@earlyclobber $t0,@earlyclobber $t1,"
                  "@earlyclobber $t2,@earlyclobber $t3" in
def ATOMIC_SUBWORD_RMW_LOOP
    : AlphaPseudo<(outs GPRC:$dst, GPRC:$t0, GPRC:$t1, GPRC:$t2, GPRC:$t3),
                  (ins GPRC:$addr, GPRC:$val, i64imm:$alu, i64imm:$isword), "",
                  []>;

// bic and zapnot, then the extract/compare half and the mask/insert/store half.
let Size = 52,
    Constraints = "@earlyclobber $dst,@earlyclobber $t0,@earlyclobber $t1,"
                  "@earlyclobber $t2,@earlyclobber $t3" in
def ATOMIC_SUBWORD_CAS_LOOP
    : AlphaPseudo<(outs GPRC:$dst, GPRC:$t0, GPRC:$t1, GPRC:$t2, GPRC:$t3),
                  (ins GPRC:$addr, GPRC:$cmp, GPRC:$new, i64imm:$isword), "",
                  []>;

// bic and insert, then ldq_l, mask, merge, stq_c and beq.
let Size = 28,
    Constraints = "@earlyclobber $t0,@earlyclobber $t1,@earlyclobber $t2" in
def SAFE_STORE_LOOP : AlphaPseudo<(outs GPRC:$t0, GPRC:$t1, GPRC:$t2),
                                  (ins GPRC:$val, GPRC:$addr, i64imm:$isword),
                                  "", []>;
}

// Subroutine return: the canonical form `ret $31, ($26), 1` is printed as the
// bare `ret` (matching gas), which also lets the assembler parse it.
// $26 is not listed as a use: the epilogue reloads it as a frame-destroy
// instruction (so it survives), and leaving it off keeps the return address,
// which is only materialized in the encoding, from reading as undefined
// before the epilogue is inserted (mirrors RISCV's PseudoRET).
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : MbrForm<0b10, (outs), (ins), "ret", [(AlphaRet)]> {
  let Ra = 31;
  let Rb = 26;
  let hint = 1;  // return-prediction hint
}

//===----------------------------------------------------------------------===//
// Branches.
//===----------------------------------------------------------------------===//

def brtarget : Operand<OtherVT> {
  let EncoderMethod = "getBranchTargetEncoding";
  let DecoderMethod = "decodeBranchTarget";
}

// Unconditional branch: `br $31, target` ($31 discards the saved PC).
let isBranch = 1, isTerminator = 1, isBarrier = 1, Ra = 31 in
def BR : BForm<0x30, (outs), (ins brtarget:$target), "br $$31, $target",
               [(br bb:$target)]>;

// Conditional branches test a register against zero.
let isBranch = 1, isTerminator = 1 in {
def BEQ : BForm<0x39, (outs), (ins GPRC:$Ra, brtarget:$target),
                "beq $Ra, $target", []>;
def BNE : BForm<0x3d, (outs), (ins GPRC:$Ra, brtarget:$target),
                "bne $Ra, $target", [(brcond GPRC:$Ra, bb:$target)]>;
}

// Indirect branch through a register (used for jump tables).
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    Ra = 31 in
def JMP : MbrForm<0b00, (outs), (ins parenGPR:$Rb), "jmp $$31, $Rb, 0", []>;
def : Pat<(brind GPRC:$r), (JMP GPRC:$r)>;
