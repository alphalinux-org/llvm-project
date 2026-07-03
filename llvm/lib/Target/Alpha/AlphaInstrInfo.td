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
// Instructions.
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {

// NOP is the canonical `bis $31, $31, $31`.
def NOP : AlphaInst<(outs), (ins), "nop", []> {
  let Opcode = 0x11;   // Integer arithmetic (operate) primary opcode.
}

// Integer register-to-register move: `bis $Ra, $Rb, $Rc` computes
// $Rc = $Ra OR $Rb.  Used by copyPhysReg as `bis $31, $Rb, $Rc`.
def BIS : AlphaInst<(outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                    "bis $Ra, $Rb, $Rc", []> {
  let Opcode = 0x11;
}

// Floating-point copy-sign: `cpys $Ra, $Rb, $Rc`.  Used by copyPhysReg as
// `cpys $Rb, $Rb, $Rc` to move a floating-point register.
def CPYS : AlphaInst<(outs FPRC:$Rc), (ins FPRC:$Ra, FPRC:$Rb),
                     "cpys $Ra, $Rb, $Rc", []> {
  let Opcode = 0x17;
}

} // hasSideEffects = 0, mayLoad = 0, mayStore = 0

// Subroutine return: `ret $31, ($26), 1`.
// $26 is not listed as a use: the epilogue reloads it as a frame-destroy
// instruction (so it survives), and leaving it off keeps the return address,
// which is only materialized in the encoding, from reading as undefined
// before the epilogue is inserted (mirrors RISCV's PseudoRET).
let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : AlphaInst<(outs), (ins), "ret $$31, ($$26), 1", [(AlphaRet)]> {
  let Opcode = 0x1a;   // JSR-format opcode (jmp/jsr/ret/jsr_coroutine).
}
