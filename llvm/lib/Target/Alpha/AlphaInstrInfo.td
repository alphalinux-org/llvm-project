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

//===----------------------------------------------------------------------===//
// Operands and immediate predicates.
//===----------------------------------------------------------------------===//

// A 16-bit signed displacement, as used by the memory-format instructions.
def s16imm : Operand<i64>;

// An immediate that fits in the 16-bit signed displacement of `lda`.
def immSExt16 : ImmLeaf<i64, [{ return isInt<16>(Imm); }]>;

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

// `bis $Ra, $Rb, $Rc` computes $Rc = $Ra OR $Rb.  copyPhysReg emits it as
// `bis $31, $Rb, $Rc` to move an integer register.
def BIS : ALU_rr<0x11, 0x20, "bis", or>;

// Floating-point copy-sign: `cpys $Ra, $Rb, $Rc`.  Used by copyPhysReg as
// `cpys $Rb, $Rb, $Rc` to move a floating-point register.
def CPYS : AlphaInst<(outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
                     "cpys $Ra, $Rb, $Rc", []> {
  let Opcode = 0x17;
}

// Load address: `lda $Rc, $disp($Rb)` computes $Rc = $Rb + sign_ext($disp).
// With $Rb = $31 (zero) this materializes a 16-bit signed constant.
def LDA : MFormD<0x08, (outs GPRC:$Ra), (ins s16imm:$disp, GPRC:$Rb),
                    "lda $Ra, $disp($Rb)", []>;

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

// Materialize a small signed constant with `lda $Rc, imm($31)`.
def : Pat<(i64 immSExt16:$imm), (LDA immSExt16:$imm, R31)>;

// Subroutine return: `ret $31, ($26), 1`.
// $26 is not listed as a use: the epilogue reloads it as a frame-destroy
// instruction (so it survives), and leaving it off keeps the return address,
// which is only materialized in the encoding, from reading as undefined
// before the epilogue is inserted (mirrors RISCV's PseudoRET).
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : AlphaInst<(outs), (ins), "ret $$31, ($$26), 1", [(AlphaRet)]> {
  let Opcode = 0x1a;   // JSR-format opcode (jmp/jsr/ret/jsr_coroutine).
}
