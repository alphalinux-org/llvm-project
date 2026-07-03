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
def memri : Operand<i64> {
  let MIOperandInfo = (ops GPRC, s16imm);
  let PrintMethod = "printMemOperand";
  let EncoderMethod = "getMemEncoding";
}

// Symbol operands whose 16-bit displacement is filled by a relocation.  The
// operand type selects the relocation: literal (GOT, R_ALPHA_LITERAL), gprel
// high and gprel low (R_ALPHA_GPRELHIGH / R_ALPHA_GPRELLOW).
class SymOperand<string enc> : Operand<i64> { let EncoderMethod = enc; }
def literal_sym : SymOperand<"getLiteralEncoding">;
def gprelhigh_sym : SymOperand<"getGprelHighEncoding">;
def gprellow_sym : SymOperand<"getGprelLowEncoding">;
// Retained alias so existing definitions read naturally.
def symbol : SymOperand<"getGprelLowEncoding">;

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

def ADDQ : ALU_rr<0x10, 0x20, "addq", add>;
def SUBQ : ALU_rr<0x10, 0x29, "subq", sub>;
def MULQ : ALU_rr<0x13, 0x20, "mulq", mul>;
def AND  : ALU_rr<0x11, 0x00, "and",  and>;
def XOR  : ALU_rr<0x11, 0x40, "xor",  xor>;

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
def FABSS : FAbs<[(set f32:$Rc, (fabs f32:$Rb))]>;
def FABST : FAbs<[(set f64:$Rc, (fabs f64:$Rb))]>;
// fneg: cpysn Rb, Rb, Rc.
class FNeg<list<dag> pat> : FSign<0x021, (ins FPRC:$Rb), "cpysn $Rb, $Rb, $Rc",
                                  pat> {
  bits<5> Rb;
  let fa = Rb;
  let fb = Rb;
}
def FNEGS : FNeg<[(set f32:$Rc, (fneg f32:$Rb))]>;
def FNEGT : FNeg<[(set f64:$Rc, (fneg f64:$Rb))]>;
// fcopysign: cpys Rb, Ra, Rc (sign of Rb, magnitude of Ra).
class FCpys<list<dag> pat>
    : FSign<0x020, (ins FPRC:$Ra, FPRC:$Rb), "cpys $Rb, $Ra, $Rc", pat> {
  bits<5> Ra;
  bits<5> Rb;
  let fa = Rb;
  let fb = Ra;
}
def FCPYSS : FCpys<[(set f32:$Rc, (fcopysign f32:$Ra, f32:$Rb))]>;
def FCPYST : FCpys<[(set f64:$Rc, (fcopysign f64:$Ra, f64:$Rb))]>;

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
def CMPTEQ : CMPT_rr<0x0a5,"cmpteq">;
def CMPTLT : CMPT_rr<0x0a6,"cmptlt">;
def CMPTLE : CMPT_rr<0x0a7,"cmptle">;

// Floating-point conditional move: `fcmovne $Fa, $Fb, $Fc` sets $Fc = $Fb when
// $Fa != 0.0.  With $Fc preset to the false value this implements an FP select.
let Constraints = "$Ffalse = $Fc" in
def FCMOVNE : AlphaInst<(outs FPRC:$Fc),
                        (ins FPRC:$Ffalse, FPRC:$Fa, FPRC:$Fb),
                        "fcmovne $Fa, $Fb, $Fc", []> {
  let Opcode = 0x17;
}

// Precision conversions between S_floating (f32) and T_floating (f64).  Both
// stay in floating-point registers.
def CVTST : AlphaInst<(outs FPRC:$Fc), (ins FPRC:$Fb), "cvtst $Fb, $Fc",
                      [(set f64:$Fc, (fpextend f32:$Fb))]> {
  let Opcode = 0x16;
}
def CVTTS : AlphaInst<(outs FPRC:$Fc), (ins FPRC:$Fb), "cvtts $Fb, $Fc",
                      [(set f32:$Fc, (fpround f64:$Fb))]> {
  let Opcode = 0x16;
}

// Integer/floating conversions on values held in floating-point registers.
def CVTQT : AlphaInst<(outs FPRC:$Fc), (ins FPRC:$Fb), "cvtqt $Fb, $Fc", []> {
  let Opcode = 0x16;
}
def CVTQS : AlphaInst<(outs FPRC:$Fc), (ins FPRC:$Fb), "cvtqs $Fb, $Fc", []> {
  let Opcode = 0x16;
}
// cvttq/c: convert T-floating to quadword with chopped (round-toward-zero)
// rounding, matching C's fp-to-integer truncation semantics.
def CVTTQ : AlphaInst<(outs FPRC:$Fc), (ins FPRC:$Fb), "cvttq/c $Fb, $Fc", []> {
  let Opcode = 0x16;
}

// Load address: `lda $Rc, $disp($Rb)` computes $Rc = $Rb + sign_ext($disp).
// With $Rb = $31 (zero) this materializes a 16-bit signed constant.
def LDA : MFormD<0x08, (outs GPRC:$Ra), (ins s16imm:$disp, GPRC:$Rb),
                    "lda $Ra, $disp($Rb)", []>;

// Load effective address, with a base+displacement operand so a frame index can
// be materialized (and rewritten by eliminateFrameIndex).
let hasSideEffects = 0 in
def LEA : MForm<0x08, (outs GPRC:$Rc), (ins memri:$addr), "lda $Rc, $addr", []>;

// Load address high: `ldah $Rc, $disp($Rb)` computes
// $Rc = $Rb + (sign_ext($disp) << 16).  Paired with lda it materializes a
// 32-bit signed constant.
def LDAH : MFormD<0x09, (outs GPRC:$Ra), (ins s16imm:$disp, GPRC:$Rb),
                     "ldah $Ra, $disp($Rb)", []>;

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
let Uses = [R27], Defs = [R29], hasSideEffects = 1, isPseudo = 1 in
def LDGP : AlphaInst<(outs), (ins), "ldgp $$29, 0($$27)", []>;

// Load a global's address from its GOT slot via an R_ALPHA_LITERAL relocation.
let mayLoad = 1, hasSideEffects = 0, Uses = [R29] in
def LDQl : AlphaInst<(outs GPRC:$dst), (ins symbol:$sym),
                     "ldq $dst, $sym($$29)\t\t!literal", []> {
  let Opcode = 0x29;
}
def : Pat<(AlphaLiteral tglobaladdr:$sym), (LDQl tglobaladdr:$sym)>;

// GP-relative high/low address parts.  The relocation suffix distinguishes the
// two; the same symbol operand feeds both.
let hasSideEffects = 0 in {
def LDAHg : MFormD<0x09, (outs GPRC:$Ra), (ins gprelhigh_sym:$disp, GPRC:$Rb),
                   "ldah $Ra, $disp($Rb)\t\t!gprelhigh", []>;
def LDAg : MFormD<0x08, (outs GPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
                  "lda $Ra, $disp($Rb)\t\t!gprellow", []>;
}
def : Pat<(AlphaGprelHi tconstpool:$sym, GPRC:$base),
          (LDAHg tconstpool:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelLo tconstpool:$sym, GPRC:$base),
          (LDAg tconstpool:$sym, GPRC:$base)>;

// A load of a GP-relative address folds the !gprellow low part into the load
// displacement, so only the ldah !gprelhigh is needed to form the address.
let mayLoad = 1, hasSideEffects = 0 in {
def LDLg : AlphaInst<(outs GPRC:$Ra), (ins symbol:$sym, GPRC:$base),
                     "ldl $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x28; }
def LDQg : AlphaInst<(outs GPRC:$Ra), (ins symbol:$sym, GPRC:$base),
                     "ldq $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x29; }
def LDTg : AlphaInst<(outs FPRC:$Ra), (ins symbol:$sym, GPRC:$base),
                     "ldt $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x23; }
def LDSg : AlphaInst<(outs FPRC:$Ra), (ins symbol:$sym, GPRC:$base),
                     "lds $Ra, $sym($base)\t\t!gprellow", []> { let Opcode = 0x22; }
}
let AddedComplexity = 20 in {
def : Pat<(i64 (sextloadi32 (AlphaGprelLo tconstpool:$s, GPRC:$b))),
          (LDLg tconstpool:$s, GPRC:$b)>;
def : Pat<(i64 (load (AlphaGprelLo tconstpool:$s, GPRC:$b))),
          (LDQg tconstpool:$s, GPRC:$b)>;
def : Pat<(f64 (load (AlphaGprelLo tconstpool:$s, GPRC:$b))),
          (LDTg tconstpool:$s, GPRC:$b)>;
def : Pat<(f32 (load (AlphaGprelLo tconstpool:$s, GPRC:$b))),
          (LDSg tconstpool:$s, GPRC:$b)>;
def : Pat<(f64 (extloadf32 (AlphaGprelLo tconstpool:$s, GPRC:$b))),
          (CVTST (LDSg tconstpool:$s, GPRC:$b))>;
}

// Call through the procedure value in $27.  jsr leaves the return address in
// $26; the following ldgp reloads the caller's global pointer, which the callee
// may have changed.  $29 is redefined with the caller's gp.
let isCall = 1, Uses = [R27], Defs = [R26, R29] in
def JSR : AlphaInst<(outs), (ins), "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                    [(AlphaCall)]> {
  let Opcode = 0x1a;
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
def LDQ_U : MFormZ<0x0b, (outs GPRC:$Ra), (ins GPRC:$base),
                   "ldq_u $Ra, 0($base)", []>;
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
def STQ_U : MFormZ<0x0f, (outs), (ins GPRC:$Ra, GPRC:$base),
                   "stq_u $Ra, 0($base)", []>;
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
// The fragment is constrained to a 64-bit access.  An unconstrained
// atomic_cmp_swap would match a cmpxchg of any width and give an i8, i16 or
// i32 one this ldq_l/stq_c loop -- the wrong access width and the wrong
// comparison.  The narrower widths get their own pseudos in later commits.
class AtomicCASvt<ValueType vt>
    : PatFrag<(ops node:$ptr, node:$cmp, node:$new),
              (atomic_cmp_swap node:$ptr, node:$cmp, node:$new)> {
  let IsAtomic = 1;
  let MemoryVT = vt;
}
let usesCustomInserter = 1, mayLoad = 1, mayStore = 1 in
def ATOMIC_CMPXCHG_I64
    : AlphaPseudo<(outs GPRC:$dst),
                  (ins GPRC:$addr, GPRC:$cmp, GPRC:$new), "",
                  [(set i64:$dst,
                        (AtomicCASvt<i64> iPTR:$addr, i64:$cmp, i64:$new))]>;

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

// bic and insert, then ldq_l, mask, merge, stq_c and beq.
let Size = 28,
    Constraints = "@earlyclobber $t0,@earlyclobber $t1,@earlyclobber $t2" in
def SAFE_STORE_LOOP : AlphaPseudo<(outs GPRC:$t0, GPRC:$t1, GPRC:$t2),
                                  (ins GPRC:$val, GPRC:$addr, i64imm:$isword),
                                  "", []>;
}

// Subroutine return: `ret $31, ($26), 1`.
// $26 is not listed as a use: the epilogue reloads it as a frame-destroy
// instruction (so it survives), and leaving it off keeps the return address,
// which is only materialized in the encoding, from reading as undefined
// before the epilogue is inserted (mirrors RISCV's PseudoRET).
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : MbrForm<0b10, (outs), (ins), "ret $$31, ($$26), 1", [(AlphaRet)]> {
  let Ra = 31;
  let Rb = 26;
  let hint = 1;  // return-prediction hint
}

//===----------------------------------------------------------------------===//
// Branches.
//===----------------------------------------------------------------------===//

def brtarget : Operand<OtherVT> {
  let EncoderMethod = "getBranchTargetEncoding";
}

// Unconditional branch: `br $31, target` ($31 discards the saved PC).
let isBranch = 1, isTerminator = 1, isBarrier = 1 in
def BR : AlphaInst<(outs), (ins brtarget:$target), "br $$31, $target",
                   [(br bb:$target)]> {
  let Opcode = 0x30;
}

// Conditional branches test a register against zero.
let isBranch = 1, isTerminator = 1 in {
def BEQ : AlphaInst<(outs), (ins GPRC:$Ra, brtarget:$target),
                    "beq $Ra, $target", []> {
  let Opcode = 0x39;
}
def BNE : AlphaInst<(outs), (ins GPRC:$Ra, brtarget:$target),
                    "bne $Ra, $target", [(brcond GPRC:$Ra, bb:$target)]> {
  let Opcode = 0x3d;
}
}
