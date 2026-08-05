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

// Eliminated by frame lowering, so no scheduler ever sees these.
let Defs = [R30], Uses = [R30], hasNoSchedulingInfo = 1 in {
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

// A tail call: jump to the callee (procedure value in $27) without saving a
// return address.
def AlphaTCReturn : SDNode<"AlphaISD::TC_RETURN", SDTNone,
                           [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// A direct call: identical, but the leading operand is the callee symbol so the
// jsr can carry the hint and lituse_jsr relocations.
def SDT_AlphaCallDirect : SDTypeProfile<0, 1, [SDTCisVT<0, i64>]>;
def AlphaCallDirect : SDNode<"AlphaISD::CALL_DIRECT", SDT_AlphaCallDirect,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;

// A direct call to a dso-local callee: the jsr carries only lituse_jsr, no
// hint, so the linker may relax it to a bsr.
def AlphaCallDirectLocal : SDNode<"AlphaISD::CALL_DIRECT_LOCAL", SDTNone,
                                  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                                   SDNPVariadic]>;

// A direct call emitted as a single bsr under -msmall-text.
def AlphaCallBsr : SDNode<"AlphaISD::CALL_BSR", SDT_AlphaCallDirect,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                           SDNPVariadic]>;

// The -msmall-text form of a tail call: a br straight to the callee.
def AlphaTCReturnBr : SDNode<"AlphaISD::TC_RETURN_BR", SDT_AlphaCallDirect,
                             [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// Calls to __tls_get_addr in the general- and local-dynamic sequences.  The jsr
// carries a lituse_tlsgd or lituse_tlsldm relocation so the linker can relax.
def AlphaCallTlsgd : SDNode<"AlphaISD::CALL_TLSGD", SDTNone,
                            [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                             SDNPVariadic]>;
def AlphaCallTlsldm : SDNode<"AlphaISD::CALL_TLSLDM", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                              SDNPVariadic]>;

// A call to a division millicode routine (__divq and friends): operands in
// $24/$25, result in $27, entered through $23.  It clobbers only a few
// registers, so it is modeled separately from a normal call.
def AlphaDivCall : SDNode<"AlphaISD::DIVCALL", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                           SDNPVariadic]>;
def AlphaOtsCall : SDNode<"AlphaISD::OTS_CALL", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue,
                           SDNPVariadic]>;

// GP-relative address materialization (small/large-data local symbols such as
// constant-pool entries): the high 16 bits via ldah !gprelhigh and the low 16
// bits via lda !gprellow.
def AlphaGprelHi : SDNode<"AlphaISD::GPREL_HI", SDTIntBinOp>;
def AlphaGprelLo : SDNode<"AlphaISD::GPREL_LO", SDTIntBinOp>;

// Single-precision integer/floating register move (itofs/ftois), produced by
// custom f32<->i32 bitcast lowering.
def AlphaMovi2fS : SDNode<"AlphaISD::MOVI2F_S",
                          SDTypeProfile<1, 1, [SDTCisVT<0, f32>, SDTCisVT<1, i64>]>>;
def AlphaMovf2iS : SDNode<"AlphaISD::MOVF2I_S",
                          SDTypeProfile<1, 1, [SDTCisVT<0, i64>, SDTCisVT<1, f32>]>>;

// Unaligned quadword load/store, ignoring the low three address bits.
def SDT_AlphaLdqU : SDTypeProfile<1, 1, [SDTCisVT<0, i64>, SDTCisPtrTy<1>]>;
def SDT_AlphaStqU : SDTypeProfile<0, 2, [SDTCisVT<0, i64>, SDTCisPtrTy<1>]>;
def AlphaLdqU : SDNode<"AlphaISD::LDQ_U", SDT_AlphaLdqU,
                       [SDNPHasChain, SDNPMayLoad, SDNPMemOperand]>;
// Misaligned store: value, pointer, byte width.
def SDT_AlphaUStore
    : SDTypeProfile<0, 3, [SDTCisVT<0, i64>, SDTCisPtrTy<1>, SDTCisVT<2, i64>]>;
def AlphaUStore : SDNode<"AlphaISD::USTORE", SDT_AlphaUStore,
                         [SDNPHasChain, SDNPMayStore, SDNPMayLoad,
                          SDNPMemOperand]>;

def AlphaStqU : SDNode<"AlphaISD::STQ_U", SDT_AlphaStqU,
                       [SDNPHasChain, SDNPMayStore, SDNPMemOperand]>;

// -msafe-partial misaligned store: value, pointer, byte width.
def SDT_AlphaSafeUStore
    : SDTypeProfile<0, 3, [SDTCisVT<0, i64>, SDTCisPtrTy<1>, SDTCisVT<2, i64>]>;
def AlphaSafeUStore : SDNode<"AlphaISD::SAFE_USTORE", SDT_AlphaSafeUStore,
                             [SDNPHasChain, SDNPMayStore, SDNPMayLoad,
                              SDNPMemOperand]>;

// Conditional branches that test a single register against zero: equal,
// not-equal, the signed relations, and the low-bit tests.
def SDTAlphaCondBr : SDTypeProfile<0, 2, [SDTCisVT<0, i64>, SDTCisVT<1, OtherVT>]>;
def AlphaBrEQ : SDNode<"AlphaISD::BR_EQ", SDTAlphaCondBr, [SDNPHasChain]>;
def AlphaBrNE : SDNode<"AlphaISD::BR_NE", SDTAlphaCondBr, [SDNPHasChain]>;
def AlphaBrLT : SDNode<"AlphaISD::BR_LT", SDTAlphaCondBr, [SDNPHasChain]>;
def AlphaBrLE : SDNode<"AlphaISD::BR_LE", SDTAlphaCondBr, [SDNPHasChain]>;
def AlphaBrGT : SDNode<"AlphaISD::BR_GT", SDTAlphaCondBr, [SDNPHasChain]>;
def AlphaBrGE : SDNode<"AlphaISD::BR_GE", SDTAlphaCondBr, [SDNPHasChain]>;

// Local-exec TLS: the thread pointer (call_pal rduniq) plus a link-time
// constant offset formed with ldah !tprelhi / lda !tprello.  The thread pointer
// itself is materialized in C++ (RDUNIQ then a copy of $0).
def AlphaTprelHi : SDNode<"AlphaISD::TPREL_HI", SDTIntBinOp>;
def AlphaTprelLo : SDNode<"AlphaISD::TPREL_LO", SDTIntBinOp>;

// Initial-exec TLS: load the thread-pointer-relative offset from the GOT with
// ldq !gottprel, then add it to the thread pointer.
def SDTAlphaGotTprel : SDTypeProfile<1, 1, [SDTCisVT<0, i64>, SDTCisPtrTy<1>]>;
def AlphaGotTprel : SDNode<"AlphaISD::GOTTPREL", SDTAlphaGotTprel>;

// General-dynamic TLS: the address of the __tls_get_addr descriptor in the GOT
// (lda !tlsgd), passed to __tls_get_addr which returns the variable's address.
def AlphaTlsgd : SDNode<"AlphaISD::TLSGD", SDTAlphaGotTprel>;

// Local-dynamic TLS: the module descriptor (lda !tlsldm) passed to
// __tls_get_addr for the module TLS base, plus the module-relative offset
// formed with ldah !dtprelhi / lda !dtprello.
def AlphaTlsldm : SDNode<"AlphaISD::TLSLDM", SDTAlphaGotTprel>;
def AlphaDtprelHi : SDNode<"AlphaISD::DTPREL_HI", SDTIntBinOp>;
def AlphaDtprelLo : SDNode<"AlphaISD::DTPREL_LO", SDTIntBinOp>;

//===----------------------------------------------------------------------===//
// Operands and immediate predicates.
//===----------------------------------------------------------------------===//

// A 16-bit signed displacement, as used by the memory-format instructions.
def s16imm : Operand<i64> {
  let EncoderMethod = "getDisp16Encoding";
}

// An immediate that fits in the 16-bit signed displacement of `lda`.
def immSExt16 : ImmLeaf<i64, [{ return isInt<16>(Imm); }]>;

// An 8-bit unsigned literal, as used by operate instructions in literal form.
def u8imm : Operand<i64> {
  let EncoderMethod = "getLit8Encoding";
}

// The 14-bit hint field of `ret', written literally rather than as a
// displacement to divide down.
def u14imm : Operand<i64>;

// An immediate that fits in the 8-bit operate literal field.
def immUExt8 : ImmLeaf<i64, [{ return isUInt<8>(Imm); }]>;

// The 26-bit function code of a PALcode call (call_pal <n>).
def PalFnAsmOperand : AsmOperandClass {
  let Name = "PalFn";
  let PredicateMethod = "isPalFn";
  let RenderMethod = "addImmOperands";
  let DiagnosticString = "PALcode function code must be a 26-bit unsigned value";
}
def palfn : Operand<i64> {
  let ParserMatchClass = PalFnAsmOperand;
}

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
def tprelhi_sym : SymOperand<"getTprelHighEncoding">;
def tprello_sym : SymOperand<"getTprelLowEncoding">;
def gottprel_sym : SymOperand<"getGotTprelEncoding">;
def tlsgd_sym : SymOperand<"getTlsgdEncoding">;
def tlsldm_sym : SymOperand<"getTlsldmEncoding">;
def dtprelhi_sym : SymOperand<"getDtprelHighEncoding">;
def dtprello_sym : SymOperand<"getDtprelLowEncoding">;
// A register printed inside parentheses, `($reg)`, as used by the jsr/jmp/ldq_u
// address forms.  Written this way (rather than literal parens in the asm
// string) so the instructions stay in the assembler matcher and disassembler.
def ParenRegAsmOperand : AsmOperandClass {
  let Name = "ParenReg";
  let PredicateMethod = "isParenReg";
  let RenderMethod = "addParenRegOperands";
  let ParserMethod = "parseMemOperand";
}
def parenGPR : RegisterOperand<GPRC> {
  let PrintMethod = "printParenReg";
  let DecoderMethod = "DecodeGPRCRegisterClass";
  let ParserMatchClass = ParenRegAsmOperand;
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

// The other misc-format barriers: write memory barrier and the trap barriers.
class MiscBarrier<bits<26> func, string mnem>
    : AlphaInst<(outs), (ins), mnem, []> {
  let Opcode = 0x18;
  let Inst{25-0} = func;
}
let hasSideEffects = 1 in {
def WMB   : MiscBarrier<0x4400, "wmb">;
def TRAPB : MiscBarrier<0x0000, "trapb">;
def EXCB  : MiscBarrier<0x0400, "excb">;
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

//===----------------------------------------------------------------------===//
// Instructions exposed to C through the __builtin_alpha_* intrinsics.
//===----------------------------------------------------------------------===//

// cmpbge compares the eight bytes of two registers; zap/zapnot clear or keep
// bytes selected by a mask; umulh is the high half of an unsigned multiply.
def CMPBGE : ALU_rr<0x10, 0x0f, "cmpbge", int_alpha_cmpbge>;
def ZAP    : ALU_rr<0x12, 0x30, "zap",    int_alpha_zap>;
def ZAPNOT : ALU_rr<0x12, 0x31, "zapnot", int_alpha_zapnot>;

// Byte/word/longword extract, insert and mask, high halves and the longword
// forms.  The byte and word low forms already exist for the pre-BWX access
// sequences and are routed to their intrinsics at the end of this file.
def EXTLL : ALU_rr<0x12, 0x26, "extll", int_alpha_extll>;
def EXTQL : ALU_rr<0x12, 0x36, "extql", int_alpha_extql>;
def EXTWH : ALU_rr<0x12, 0x5a, "extwh", int_alpha_extwh>;
def EXTLH : ALU_rr<0x12, 0x6a, "extlh", int_alpha_extlh>;
def EXTQH : ALU_rr<0x12, 0x7a, "extqh", int_alpha_extqh>;
def INSLL : ALU_rr<0x12, 0x2b, "insll", int_alpha_insll>;
def INSQL : ALU_rr<0x12, 0x3b, "insql", int_alpha_insql>;
def INSWH : ALU_rr<0x12, 0x57, "inswh", int_alpha_inswh>;
def INSLH : ALU_rr<0x12, 0x67, "inslh", int_alpha_inslh>;
def INSQH : ALU_rr<0x12, 0x77, "insqh", int_alpha_insqh>;
def MSKLL : ALU_rr<0x12, 0x22, "mskll", int_alpha_mskll>;
def MSKQL : ALU_rr<0x12, 0x32, "mskql", int_alpha_mskql>;
def MSKWH : ALU_rr<0x12, 0x52, "mskwh", int_alpha_mskwh>;
def MSKLH : ALU_rr<0x12, 0x62, "msklh", int_alpha_msklh>;
def MSKQH : ALU_rr<0x12, 0x72, "mskqh", int_alpha_mskqh>;

// Motion video instructions (MVI, enabled by the mvi feature): packed byte/word min and max, the sum
// of absolute byte differences, and pack/unpack between bytes and wider lanes.
let Predicates = [HasMVI] in {
def MINSB8 : ALU_rr<0x1c, 0x38, "minsb8", int_alpha_minsb8>;
def MINSW4 : ALU_rr<0x1c, 0x39, "minsw4", int_alpha_minsw4>;
def MINUB8 : ALU_rr<0x1c, 0x3a, "minub8", int_alpha_minub8>;
def MINUW4 : ALU_rr<0x1c, 0x3b, "minuw4", int_alpha_minuw4>;
def MAXUB8 : ALU_rr<0x1c, 0x3c, "maxub8", int_alpha_maxub8>;
def MAXUW4 : ALU_rr<0x1c, 0x3d, "maxuw4", int_alpha_maxuw4>;
def MAXSB8 : ALU_rr<0x1c, 0x3e, "maxsb8", int_alpha_maxsb8>;
def MAXSW4 : ALU_rr<0x1c, 0x3f, "maxsw4", int_alpha_maxsw4>;
def PERR   : ALU_rr<0x1c, 0x31, "perr",   int_alpha_perr>;

// The pack/unpack forms take a single source in Rb with Ra fixed to $31.
class MVIUnary<bits<7> func, string mnem, SDPatternOperator node>
    : OForm<0x1c, func, (outs GPRC:$Rc), (ins GPRC:$Rb), mnem # " $Rb, $Rc",
            [(set i64:$Rc, (node i64:$Rb))]> {
  let Ra = 31;
}
def UNPKBW : MVIUnary<0x34, "unpkbw", int_alpha_unpkbw>;
def UNPKBL : MVIUnary<0x35, "unpkbl", int_alpha_unpkbl>;
def PKWB   : MVIUnary<0x36, "pkwb",   int_alpha_pkwb>;
def PKLB   : MVIUnary<0x37, "pklb",   int_alpha_pklb>;
} // Predicates = [HasMVI]

// amask clears the feature bits its argument names that the processor
// implements, reporting which are absent.  Ra is fixed to $31.
let hasSideEffects = 0 in
def AMASK : OForm<0x11, 0x61, (outs GPRC:$Rc), (ins GPRC:$Rb), "amask $Rb, $Rc",
                  [(set i64:$Rc, (int_alpha_amask i64:$Rb))]> {
  let Ra = 31;
}

// implver reports the processor implementation version as a small constant.
// It is encoded as an operate with $31 and a literal of 1.
let hasSideEffects = 0 in
def IMPLVER : AlphaInst<(outs GPRC:$Rc), (ins), "implver $Rc",
                        [(set i64:$Rc, (int_alpha_implver))]> {
  bits<5> Rc;
  let Opcode = 0x11;
  let Inst{25-21} = 31;
  let Inst{20-13} = 1;
  let Inst{12} = 1;
  let Inst{11-5} = 0x6c;
  let Inst{4-0} = Rc;
}

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
// Longword forms scale then sign-extend the low 32 bits, folding the sextl an
// i32-typed scaled add/subtract would otherwise need.
def S4ADDL : OForm<0x10, 0x02, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s4addl $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sext_inreg (add (shl i64:$Ra, (i64 2)),
                                                   i64:$Rb), i32))]>;
def S8ADDL : OForm<0x10, 0x12, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s8addl $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sext_inreg (add (shl i64:$Ra, (i64 3)),
                                                   i64:$Rb), i32))]>;
def S4SUBL : OForm<0x10, 0x0b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s4subl $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sext_inreg (sub (shl i64:$Ra, (i64 2)),
                                                   i64:$Rb), i32))]>;
def S8SUBL : OForm<0x10, 0x1b, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "s8subl $Ra, $Rb, $Rc",
                   [(set i64:$Rc, (sext_inreg (sub (shl i64:$Ra, (i64 3)),
                                                   i64:$Rb), i32))]>;
// Literal forms of scaled add/subtract (assembler-only; no DAG patterns needed
// since the compiler always uses register forms from the patterns above).
def S4ADDQi : OFormL<0x10, 0x22, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s4addq $Ra, $lit, $Rc", []>;
def S8ADDQi : OFormL<0x10, 0x32, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s8addq $Ra, $lit, $Rc", []>;
def S4SUBQi : OFormL<0x10, 0x2b, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s4subq $Ra, $lit, $Rc", []>;
def S8SUBQi : OFormL<0x10, 0x3b, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s8subq $Ra, $lit, $Rc", []>;
def S4ADDLi : OFormL<0x10, 0x02, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s4addl $Ra, $lit, $Rc", []>;
def S8ADDLi : OFormL<0x10, 0x12, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s8addl $Ra, $lit, $Rc", []>;
def S4SUBLi : OFormL<0x10, 0x0b, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s4subl $Ra, $lit, $Rc", []>;
def S8SUBLi : OFormL<0x10, 0x1b, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                     "s8subl $Ra, $lit, $Rc", []>;
}

// Literal (8-bit immediate) form of an operate instruction:
// `<mnem> $Ra, lit, $Rc` computes $Rc = $Ra <op> zext(lit).
class ALU_ri<bits<6> opc, bits<7> func, string mnem, SDPatternOperator node>
    : OFormL<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc",
             [(set i64:$Rc, (node i64:$Ra, immUExt8:$lit))]>;
// Longword form: the 32-bit result is sign-extended, folding the sextl.
class ALU_riL<bits<6> opc, bits<7> func, string mnem, SDPatternOperator node>
    : OFormL<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc",
             [(set i64:$Rc, (sext_inreg (node i64:$Ra, immUExt8:$lit), i32))]>;

class ALU_ri_nopat<bits<6> opc, bits<7> func, string mnem>
    : OFormL<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc", []>;

def ADDQi : ALU_ri<0x10, 0x20, "addq", add>;
def MULQi : ALU_ri<0x13, 0x20, "mulq", mul>;
def ADDLi : ALU_riL<0x10, 0x00, "addl", add>;
// The subtract-a-constant forms carry no pattern: DAGCombine canonicalizes
// `sub x, C' to `add x, -C' before instruction selection, so one would never
// be matched.  They exist for hand-written assembly.
def SUBQi : ALU_ri_nopat<0x10, 0x29, "subq">;
def SUBLi : ALU_ri_nopat<0x10, 0x09, "subl">;
def MULLi : ALU_riL<0x13, 0x00, "mull", mul>;
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

// Longword add/subtract/multiply: the 32-bit result is sign-extended to 64
// bits, which is exactly an i32 operation, so they fold the sextl the quadword
// forms would otherwise need.  addl with $Rb = $31 also sign-extends $Ra.
def ADDL : OForm<0x10, 0x00, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                 "addl $Ra, $Rb, $Rc",
                 [(set i64:$Rc, (sext_inreg (add i64:$Ra, i64:$Rb), i32))]>;
def SUBL : OForm<0x10, 0x09, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                 "subl $Ra, $Rb, $Rc",
                 [(set i64:$Rc, (sext_inreg (sub i64:$Ra, i64:$Rb), i32))]>;
def MULL : OForm<0x13, 0x00, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                 "mull $Ra, $Rb, $Rc",
                 [(set i64:$Rc, (sext_inreg (mul i64:$Ra, i64:$Rb), i32))]>;

// Comparisons write 0 or 1 into $Rc (Alpha has no condition-code register).
class CMP_rr<bits<7> func, string mnem>
    : OForm<0x10, func, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
            mnem # " $Ra, $Rb, $Rc", []>;
def CMPEQ  : CMP_rr<0x2d, "cmpeq">;
def CMPLT  : CMP_rr<0x4d, "cmplt">;
def CMPLE  : CMP_rr<0x6d, "cmple">;
def CMPULT : CMP_rr<0x1d, "cmpult">;
def CMPULE : CMP_rr<0x3d, "cmpule">;

// Immediate form: compare against an 8-bit unsigned literal (so a comparison
// against a small constant, in particular zero, needs no separate materialize).
class CMP_ri<bits<7> func, string mnem>
    : OFormL<0x10, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc", []>;
def CMPEQi  : CMP_ri<0x2d, "cmpeq">;
def CMPLTi  : CMP_ri<0x4d, "cmplt">;
def CMPLEi  : CMP_ri<0x6d, "cmple">;
def CMPULTi : CMP_ri<0x1d, "cmpult">;
def CMPULEi : CMP_ri<0x3d, "cmpule">;

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

// A plain register move: cpys Rb, Rb, Rc.
class FMov<list<dag> pat> : FSign<0x020, (ins FPRC:$Rb), "cpys $Rb, $Rb, $Rc",
                                  pat> {
  bits<5> Rb;
  let fa = Rb;
  let fb = Rb;
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
            [(set vt:$Rc, (node vt:$Ra, vt:$Rb))]> {
  let TrapClass = 1;  // Arithmetic: su / sui.
}

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
            mnem # " $Ra, $Rb, $Rc", []> {
  let TrapClass = 2;  // Compare: su only.
}
def CMPTUN : CMPT_rr<0x0a4, "cmptun">;
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
// A float loaded with lds is already held in T_floating form, so widening one
// to double is a register move.  cvtst would fault on an infinity or a NaN
// unless it asks for software completion, so it is only used under -mieee,
// where it also handles a denormal properly.  GCC makes the same split.
let TrapClass = 5 in
def CVTST : FP_r<0x16, 0x2ac, "cvtst", []>;
let isCodeGenOnly = 1, Predicates = [NotIEEE] in
def FPEXTST : FMov<[(set f64:$Rc, (fpextend f32:$Rb))]>;
// cvtst/s: S-to-T with software completion.  This is what -mieee selects for a
// widening (see the fpextend pattern below), and it is what hand-written asm
// under -mieee uses as well (e.g. glibc s_rintf.c).  It is a mnemonic of its
// own, not cvtst with a qualifier: 0x6ac is 0x2ac with the s bit already in it,
// and cvtst's trap-mode position is occupied -- so it carries no TrapClass and
// the encoder leaves its function field alone.
def CVTST_S : FP_r<0x16, 0x6ac, "cvtst/s", []>;
let TrapClass = 1 in  // T-to-S convert is arithmetic: su / sui.
def CVTTS : FP_r<0x16, 0x0ac, "cvtts", [(set f32:$Rc, (fpround f64:$Rb))]>;

// Square root, from the FIX extension (ev6).
let Predicates = [HasFIX], hasSideEffects = 0, TrapClass = 1 in {
def SQRTS : FP_r<0x14, 0x08b, "sqrts", [(set f32:$Rc, (fsqrt f32:$Rb))]>;
def SQRTT : FP_r<0x14, 0x0ab, "sqrtt", [(set f64:$Rc, (fsqrt f64:$Rb))]>;
}

// Integer/floating conversions on values held in floating-point registers.
// Integer-to-float can only raise inexact, so it takes the sui qualifier only.
let TrapClass = 4 in {
def CVTQT : FP_r<0x16, 0x0be, "cvtqt", []>;
def CVTQS : FP_r<0x16, 0x0bc, "cvtqs", []>;
}
// cvtqt/d: integer-to-float with dynamic rounding mode (from FPCR).
// Used in hand-written asm under -mfp-rounding-mode=d (e.g. glibc s_rint.c).
def CVTQT_D : FP_r<0x16, 0x0fe, "cvtqt/d", []>;

// cvttq/c: convert T-floating to quadword with chopped (round-toward-zero)
// rounding, matching C's fp-to-integer truncation semantics.  Float-to-int
// takes the sv / svi qualifier.
let TrapClass = 3 in {
def CVTTQ : FP_r<0x16, 0x02f, "cvttq/c", []>;
// The same conversion rounding to nearest, which hand-written assembly may ask
// for and GNU as accepts.  Codegen never selects it -- C truncates -- but the
// assembler has to be able to say what the architecture can encode, and the
// disassembler to read it back.
def CVTTQn : FP_r<0x16, 0x0af, "cvttq", []>;
}

// cvttq/svid: float-to-integer with software completion, overflow, inexact,
// and dynamic rounding mode.  Used in hand-written asm under -mieee-with-inexact
// -mfp-rounding-mode=d (e.g. glibc s_rint.c).
def CVTTQ_SVID : FP_r<0x16, 0x7ef, "cvttq/svid", []>;

// Register-register conditional moves with no pattern of their own: the low
// bit tests, which nothing selects, and the two conditions DAG canonicalization
// rewrites away (setge/setle against zero become setgt/setlt against -1/1), so
// the compiler never emits them but hand-written assembly and the platform's
// own libraries are full of them.
def CMOVLBS : OForm<0x11, 0x14, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                    "cmovlbs $Ra, $Rb, $Rc", []>;
def CMOVLBC : OForm<0x11, 0x16, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                    "cmovlbc $Ra, $Rb, $Rc", []>;
def CMOVGE : OForm<0x11, 0x46, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "cmovge $Ra, $Rb, $Rc", []>;
def CMOVLE : OForm<0x11, 0x64, (outs GPRC:$Rc), (ins GPRC:$Ra, GPRC:$Rb),
                   "cmovle $Ra, $Rb, $Rc", []>;
// Move the floating-point control register to/from a floating register.  The
// operand is encoded in all three register fields.  These carry no pattern and
// exist only so hand-written assembly (context save/restore) assembles.
let hasSideEffects = 1, DecoderMethod = "decodeFpcrMove" in {
def MF_FPCR : FForm<0x17, 0x025, (outs FPRC:$reg), (ins), "mf_fpcr $reg", []> {
  bits<5> reg;
  let Ra = reg;
  let Rb = reg;
  let Rc = reg;
}
def MT_FPCR : FForm<0x17, 0x024, (outs), (ins FPRC:$reg), "mt_fpcr $reg", []> {
  bits<5> reg;
  let Ra = reg;
  let Rb = reg;
  let Rc = reg;
}
}

// Load address: `lda $Rc, $disp($Rb)` computes $Rc = $Rb + sign_ext($disp).
// With $Rb = $31 (zero) this materializes a 16-bit signed constant.  The
// split-operand form is used by codegen; LEA/LDAHm below carry the single
// memory operand that the assembler and disassembler parse and print.
// Rematerializable: with a constant base ($31) these compute a constant, so the
// register allocator recomputes them at a use rather than spilling and
// reloading (the per-instance check rejects the frame-relative uses).
let isCodeGenOnly = 1, isReMaterializable = 1, isAsCheapAsAMove = 1 in {
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

// Under -mieee the widening goes through cvtst/s, which is completed in
// software; without it the register move above is used instead.  The /s form
// is its own mnemonic rather than a qualifier on cvtst -- cvtst's function
// field already carries 0x200 in the trap-mode position, which is what tells it
// from cvtts -- so it is selected outright and not left to the encoder.
let Predicates = [HasIEEE] in
def : Pat<(f64 (fpextend f32:$Rb)), (CVTST_S $Rb)>;

// Materialize a small signed constant with `lda $Rc, imm($31)`.
def : Pat<(i64 immSExt16:$imm), (LDA immSExt16:$imm, R31)>;

// lda computes $Rb + sext16(disp) with no side effects, so it is an add of any
// constant the 16-bit displacement holds.  Without this, every x + C outside
// the 8-bit operate literal costs a materialization and an addq, and an extra
// short-lived register, where GCC uses the one lda.  DAGCombine has already
// turned every constant subtract into an add of the negation, so this covers
// x - C as well.  The predicate excludes what ADDQi already folds, so the two
// patterns do not compete.
def immSExt16NotU8 : ImmLeaf<i64, [{
  return isInt<16>(Imm) && !isUInt<8>(Imm);
}]>;
def : Pat<(add GPRC:$Rb, immSExt16NotU8:$disp),
          (LDA immSExt16NotU8:$disp, GPRC:$Rb)>;

// `not $x` is `ornot $31, $x` (0 OR ~x), avoiding an explicit -1.
def : Pat<(not GPRC:$x), (ORNOT R31, GPRC:$x)>;

// Negate is `subq $31, $x` (0 - x); keep the zero in the register file rather
// than materializing it with an lda.
def : Pat<(sub (i64 0), GPRC:$x), (SUBQ R31, GPRC:$x)>;

// Multiply by 3 is 4x - x, a single scaled subtract.  (Multiplies by 5, 7 and
// 9 are split by decomposeMulByConstant into shift+add/sub forms that fold into
// s4addq/s8subq/s8addq, so they need no pattern here.)
def : Pat<(mul GPRC:$x, 3), (S4SUBQ GPRC:$x, GPRC:$x)>;
def : Pat<(sext_inreg (mul GPRC:$x, 3), i32), (S4SUBL GPRC:$x, GPRC:$x)>;

//===----------------------------------------------------------------------===//
// Global pointer and global addresses.
//===----------------------------------------------------------------------===//

// Establish the global pointer from the procedure value on entry.  `ldgp` is a
// gas macro that expands to an ldah/lda pair with matching !gpdisp relocations.
let Uses = [R27], Defs = [R29], hasSideEffects = 1, isPseudo = 1, Size = 8 in
def LDGP : AlphaInst<(outs), (ins), "ldgp $$29, 0($$27)", []>;

// Re-establish the global pointer at an exception handler.  A landing pad is
// entered from the unwinder rather than by falling out of the invoke's jsr, so
// the ldgp that follows the call never runs on the unwind path and $29 still
// holds the unwinder's own global pointer.
//
// Nothing incoming can be used to rebuild it.  libgcc's __builtin_eh_return
// leaves the handler's address in $26 (EH_RETURN_HANDLER_RTX aliases the saved
// return address, which the epilogue restores and returns through), which is
// what gcc's landing pads rely on, but libunwind's Registers_alpha::jumpto
// jumps through $28 and restores $26 from the frame's own unwind info.  `br`
// writes the address of the next instruction into $29 unconditionally, so the
// !gpdisp pair that follows resolves against a base it established itself.
//
// The branch doubles as the padding gcc emits as `unop` (alpha_pad_function_end):
// a landing pad usually follows a call to a noreturn function such as
// __cxa_throw, and a gp reload directly adjacent to such a call can be deleted
// by the linker's relaxation.
let Defs = [R29], hasSideEffects = 1, isPseudo = 1, isCodeGenOnly = 1,
    Size = 12 in
def LDGPself : AlphaInst<(outs), (ins), "br $$29, 1f\n1:\tldgp $$29, 0($$29)",
                         []>;

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
def : Pat<(AlphaGprelHi tblockaddress:$sym, GPRC:$base),
          (LDAHg tblockaddress:$sym, GPRC:$base)>;
def : Pat<(AlphaGprelLo tblockaddress:$sym, GPRC:$base),
          (LDAg tblockaddress:$sym, GPRC:$base)>;

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
let isCodeGenOnly = 1 in
def LDSgf64 : MFormD<0x22, (outs FPRC:$Ra), (ins gprellow_sym:$disp, GPRC:$Rb),
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
let Predicates = [NotIEEE] in
def : Pat<(f64 (extloadf32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (LDSgf64 sym:$s, GPRC:$b)>;
let Predicates = [HasIEEE] in
def : Pat<(f64 (extloadf32 (AlphaGprelLo sym:$s, GPRC:$b))),
          (CVTST_S (LDSg sym:$s, GPRC:$b))>;
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

//===----------------------------------------------------------------------===//
// Thread-local storage (local-exec model).
//===----------------------------------------------------------------------===//

// Read the thread pointer into $0 via the rduniq PALcall.
let Defs = [R0], hasSideEffects = 1, isCodeGenOnly = 1 in
def RDUNIQ : PalForm<0x9e, (outs), (ins), "call_pal 0x9e", []>;

// Write the thread pointer from $0 via the wrunique PALcall.
let Uses = [R0], hasSideEffects = 1, isCodeGenOnly = 1 in
def WRUNIQ : PalForm<0x9f, (outs), (ins), "call_pal 0x9f", []>;

//===----------------------------------------------------------------------===//
// PALcode calls (assembler support).
//===----------------------------------------------------------------------===//

// The general form: call_pal with an explicit 26-bit function code.  The kernel
// invokes privileged PALcode this way, passing the function number by value.
let hasSideEffects = 1 in
def CALL_PAL : PalCallForm<(outs), (ins palfn:$func), "call_pal $func", []>;

// The PALcode functions GNU as gives mnemonics of their own.  Each is just
// call_pal with a fixed function code.  Function numbers below 0x80 are
// privileged and only a kernel may issue them -- halt and draina here -- while
// 0x80 and above are callable from user code.
class PalAlias<string mnem, int func> : InstAlias<mnem, (CALL_PAL func)>;
def : PalAlias<"halt",    0x0000>;
def : PalAlias<"draina",  0x0002>;
def : PalAlias<"bpt",     0x0080>;
def : PalAlias<"bugchk",  0x0081>;
def : PalAlias<"callsys", 0x0083>;
def : PalAlias<"chmk",    0x0083>;
def : PalAlias<"imb",     0x0086>;
def : PalAlias<"rduniq",  0x009e>;
def : PalAlias<"wruniq",  0x009f>;
def : PalAlias<"gentrap", 0x00aa>;

// Thread-pointer-relative address parts: ldah !tprelhi / lda !tprello, formed
// against the thread pointer instead of the global pointer.
let hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDAHt : MFormD<0x09, (outs GPRC:$Ra), (ins tprelhi_sym:$disp, GPRC:$Rb),
                   "ldah $Ra, $disp($Rb)\t\t!tprelhi", []>;
def LDAt : MFormD<0x08, (outs GPRC:$Ra), (ins tprello_sym:$disp, GPRC:$Rb),
                  "lda $Ra, $disp($Rb)\t\t!tprello", []>;
}
def : Pat<(AlphaTprelHi tglobaltlsaddr:$sym, GPRC:$base),
          (LDAHt tglobaltlsaddr:$sym, GPRC:$base)>;
def : Pat<(AlphaTprelLo tglobaltlsaddr:$sym, GPRC:$base),
          (LDAt tglobaltlsaddr:$sym, GPRC:$base)>;

// Initial-exec: load the tp-relative offset from the GOT (ldq !gottprel).
let mayLoad = 1, hasSideEffects = 0, Uses = [R29], isCodeGenOnly = 1 in
def LDQtp : MFormD<0x29, (outs GPRC:$Ra), (ins gottprel_sym:$disp),
                   "ldq $Ra, $disp($$29)\t\t!gottprel", []> {
  let Rb = 29;
}
def : Pat<(AlphaGotTprel tglobaltlsaddr:$sym), (LDQtp tglobaltlsaddr:$sym)>;

// General-dynamic: form the address of the tlsgd descriptor (lda !tlsgd).
let hasSideEffects = 0, Uses = [R29], isCodeGenOnly = 1 in
def LDAtlsgd : MFormD<0x08, (outs GPRC:$Ra), (ins tlsgd_sym:$disp),
                      "lda $Ra, $disp($$29)\t\t!tlsgd", []> {
  let Rb = 29;
}
def : Pat<(AlphaTlsgd tglobaltlsaddr:$sym), (LDAtlsgd tglobaltlsaddr:$sym)>;

// Local-dynamic: the module descriptor (lda !tlsldm), then the module-relative
// offset (ldah !dtprelhi / lda !dtprello, folding the low part into loads).
let hasSideEffects = 0, Uses = [R29], isCodeGenOnly = 1 in
def LDAtlsldm : MFormD<0x08, (outs GPRC:$Ra), (ins tlsldm_sym:$disp),
                       "lda $Ra, $disp($$29)\t\t!tlsldm", []> {
  let Rb = 29;
}
def : Pat<(AlphaTlsldm tglobaltlsaddr:$sym), (LDAtlsldm tglobaltlsaddr:$sym)>;

let hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDAHd : MFormD<0x09, (outs GPRC:$Ra), (ins dtprelhi_sym:$disp, GPRC:$Rb),
                   "ldah $Ra, $disp($Rb)\t\t!dtprelhi", []>;
def LDAd : MFormD<0x08, (outs GPRC:$Ra), (ins dtprello_sym:$disp, GPRC:$Rb),
                  "lda $Ra, $disp($Rb)\t\t!dtprello", []>;
}
def : Pat<(AlphaDtprelHi tglobaltlsaddr:$sym, GPRC:$base),
          (LDAHd tglobaltlsaddr:$sym, GPRC:$base)>;
def : Pat<(AlphaDtprelLo tglobaltlsaddr:$sym, GPRC:$base),
          (LDAd tglobaltlsaddr:$sym, GPRC:$base)>;

let mayLoad = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDLd : MFormD<0x28, (outs GPRC:$Ra), (ins dtprello_sym:$disp, GPRC:$Rb),
                  "ldl $Ra, $disp($Rb)\t\t!dtprello", []>;
def LDQd : MFormD<0x29, (outs GPRC:$Ra), (ins dtprello_sym:$disp, GPRC:$Rb),
                  "ldq $Ra, $disp($Rb)\t\t!dtprello", []>;
def LDTd : MFormD<0x23, (outs FPRC:$Ra), (ins dtprello_sym:$disp, GPRC:$Rb),
                  "ldt $Ra, $disp($Rb)\t\t!dtprello", []>;
def LDSd : MFormD<0x22, (outs FPRC:$Ra), (ins dtprello_sym:$disp, GPRC:$Rb),
                  "lds $Ra, $disp($Rb)\t\t!dtprello", []>;
}
let mayStore = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def STLd : MFormD<0x2c, (outs), (ins GPRC:$Ra, dtprello_sym:$disp, GPRC:$Rb),
                  "stl $Ra, $disp($Rb)\t\t!dtprello", []>;
def STQd : MFormD<0x2d, (outs), (ins GPRC:$Ra, dtprello_sym:$disp, GPRC:$Rb),
                  "stq $Ra, $disp($Rb)\t\t!dtprello", []>;
def STTd : MFormD<0x27, (outs), (ins FPRC:$Ra, dtprello_sym:$disp, GPRC:$Rb),
                  "stt $Ra, $disp($Rb)\t\t!dtprello", []>;
def STSd : MFormD<0x26, (outs), (ins FPRC:$Ra, dtprello_sym:$disp, GPRC:$Rb),
                  "sts $Ra, $disp($Rb)\t\t!dtprello", []>;
}
let AddedComplexity = 20 in {
def : Pat<(i64 (sextloadi32 (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDLd tglobaltlsaddr:$s, GPRC:$b)>;
// A longword load that is not sign-extending needs the same fold: an extending
// load of don't-care high bits is the sign-extending instruction all the same,
// and a zero-extending one is that instruction with the high bits cleared
// afterwards.
def : Pat<(i64 (extloadi32 (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDLd tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(i64 (zextloadi32 (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (ZAPNOTi (LDLd tglobaltlsaddr:$s, GPRC:$b), 15)>;
def : Pat<(i64 (load (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDQd tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(f64 (load (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDTd tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(f32 (load (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDSd tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(truncstorei32 GPRC:$v, (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STLd GPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store GPRC:$v, (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STQd GPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store f64:$v, (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STTd FPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store f32:$v, (AlphaDtprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STSd FPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
}

// A load from a thread-pointer-relative address folds the !tprello low part
// into the load displacement, as for the GP-relative case.
let mayLoad = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def LDLt : MFormD<0x28, (outs GPRC:$Ra), (ins tprello_sym:$disp, GPRC:$Rb),
                  "ldl $Ra, $disp($Rb)\t\t!tprello", []>;
def LDQt : MFormD<0x29, (outs GPRC:$Ra), (ins tprello_sym:$disp, GPRC:$Rb),
                  "ldq $Ra, $disp($Rb)\t\t!tprello", []>;
def LDTt : MFormD<0x23, (outs FPRC:$Ra), (ins tprello_sym:$disp, GPRC:$Rb),
                  "ldt $Ra, $disp($Rb)\t\t!tprello", []>;
def LDSt : MFormD<0x22, (outs FPRC:$Ra), (ins tprello_sym:$disp, GPRC:$Rb),
                  "lds $Ra, $disp($Rb)\t\t!tprello", []>;
}
let mayStore = 1, hasSideEffects = 0, isCodeGenOnly = 1 in {
def STLt : MFormD<0x2c, (outs), (ins GPRC:$Ra, tprello_sym:$disp, GPRC:$Rb),
                  "stl $Ra, $disp($Rb)\t\t!tprello", []>;
def STQt : MFormD<0x2d, (outs), (ins GPRC:$Ra, tprello_sym:$disp, GPRC:$Rb),
                  "stq $Ra, $disp($Rb)\t\t!tprello", []>;
def STTt : MFormD<0x27, (outs), (ins FPRC:$Ra, tprello_sym:$disp, GPRC:$Rb),
                  "stt $Ra, $disp($Rb)\t\t!tprello", []>;
def STSt : MFormD<0x26, (outs), (ins FPRC:$Ra, tprello_sym:$disp, GPRC:$Rb),
                  "sts $Ra, $disp($Rb)\t\t!tprello", []>;
}
let AddedComplexity = 20 in {
def : Pat<(i64 (sextloadi32 (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDLt tglobaltlsaddr:$s, GPRC:$b)>;
// A longword load that is not sign-extending needs the same fold: an extending
// load of don't-care high bits is the sign-extending instruction all the same,
// and a zero-extending one is that instruction with the high bits cleared
// afterwards.
def : Pat<(i64 (extloadi32 (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDLt tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(i64 (zextloadi32 (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (ZAPNOTi (LDLt tglobaltlsaddr:$s, GPRC:$b), 15)>;
def : Pat<(i64 (load (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDQt tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(f64 (load (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDTt tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(f32 (load (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b))),
          (LDSt tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(truncstorei32 GPRC:$v, (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STLt GPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store GPRC:$v, (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STQt GPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store f64:$v, (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STTt FPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
def : Pat<(store f32:$v, (AlphaTprelLo tglobaltlsaddr:$s, GPRC:$b)),
          (STSt FPRC:$v, tglobaltlsaddr:$s, GPRC:$b)>;
}

// Call through the procedure value in $27.  jsr leaves the return address in
// $26; the following ldgp reloads the caller's global pointer, which the callee
// may have changed.  $29 is redefined with the caller's gp.
// Size is 4 (jsr) + 8 (the ldgp expansion, see AlphaMCCodeEmitter::emitLdgp).
let isCall = 1, Uses = [R27], Defs = [R26, R29], isCodeGenOnly = 1, Size = 12 in
def JSR : MbrForm<0b01, (outs), (ins), "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                  [(AlphaCall)]> {
  let Ra = 26;
  let Rb = 27;
}

// The call target of a direct jsr, carried only to attach relocations: the
// 14-bit hint field is filled from it (R_ALPHA_HINT) and a lituse_jsr
// relocation is emitted alongside.
def calltarget : Operand<i64> {
  let EncoderMethod = "getHintEncoding";
}

// A direct call: same as JSR but tagged with the callee symbol.
let isCall = 1, Uses = [R27], Defs = [R26, R29], isCodeGenOnly = 1,
    Size = 12 in
def JSRd : MbrForm<0b01, (outs), (ins calltarget:$sym),
                   "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)", []> {
  bits<14> sym;
  let Ra = 26;
  let Rb = 27;
  let hint = sym;
}
def : Pat<(AlphaCallDirect tglobaladdr:$sym), (JSRd tglobaladdr:$sym)>;
def : Pat<(AlphaCallDirect texternalsym:$sym), (JSRd texternalsym:$sym)>;

// A dso-local direct call: only the lituse_jsr relocation, emitted in the code
// emitter; no callee operand is needed since lituse references the text section.
let isCall = 1, Uses = [R27], Defs = [R26, R29], isCodeGenOnly = 1,
    Size = 12 in
def JSRdl : MbrForm<0b01, (outs), (ins),
                    "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                    [(AlphaCallDirectLocal)]> {
  let Ra = 26;
  let Rb = 27;
}

// The jsr to __tls_get_addr in a general- or local-dynamic sequence.  Encoded
// like JSR, but the code emitter attaches a lituse_tlsgd or lituse_tlsldm
// relocation so the linker can relax the sequence.
let isCall = 1, Uses = [R27], Defs = [R26, R29], isCodeGenOnly = 1,
    Size = 12 in {
def JSRtlsgd : MbrForm<0b01, (outs), (ins),
                       "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                       [(AlphaCallTlsgd)]> {
  let Ra = 26;
  let Rb = 27;
}
def JSRtlsldm : MbrForm<0b01, (outs), (ins),
                        "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                        [(AlphaCallTlsldm)]> {
  let Ra = 26;
  let Rb = 27;
}
}

// A bare jsr word (no ldgp reload); the assembler emits it for `jsr $Ra, ($Rb)`
// and the disassembler decodes a jsr word with no branch-prediction hint to it.
let isCall = 1 in
def JSRr : MbrForm<0b01, (outs), (ins GPRC:$Ra, parenGPR:$Rb),
                   "jsr $Ra, $Rb", []>;

// The call target of a jsr written in assembly, which only carries relocations:
// an R_ALPHA_HINT filling the branch-prediction field, preceded by an
// R_ALPHA_LITUSE when a !lituse_jsr suffix marks the call as the use of a GOT
// literal.  A plain number, as in `jsr $26, ($27), 0`, encodes no relocation.
def jsrtarget : Operand<i64> {
  let EncoderMethod = "getCallTargetEncoding";
}

// jsr $Ra, ($Rb), target: the three-operand form hand-written and GNU-generated
// assembly uses.  The target does not change the encoding beyond the hint field
// the relocations fill in, so nothing decodes to this form: a jsr word reaches
// JSRr, and only when its hint field is zero.
let isCall = 1, isAsmParserOnly = 1 in
def JSRt : MbrForm<0b01, (outs), (ins GPRC:$Ra, parenGPR:$Rb, jsrtarget:$sym),
                   "jsr $Ra, $Rb, $sym", []> {
  bits<14> sym;
  let hint = sym;
}

// jmp $Ra, ($Rb), target: the tail-call spelling of the same thing.
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    isAsmParserOnly = 1 in
def JMPt : MbrForm<0b00, (outs), (ins GPRC:$Ra, parenGPR:$Rb, jsrtarget:$sym),
                   "jmp $Ra, $Rb, $sym", []> {
  bits<14> sym;
  let hint = sym;
}

// Division millicode call: enter __divq etc. through $23; the routine preserves
// all but a handful of registers, so no ldgp reload is needed afterwards.
let isCall = 1, Uses = [R24, R25, R27],
    Defs = [R23, R24, R25, R27, R28], isCodeGenOnly = 1 in
def DIVCALL : MbrForm<0b01, (outs), (ins), "jsr $$23, ($$27)", [(AlphaDivCall)]> {
  let Ra = 23;
  let Rb = 27;
}

// OTS floating-point library call: a standard jsr $26,($27) + ldgp reload,
// used for _OtsAddX/_OtsSubX/_OtsMulX/_OtsDivX and the other X_floating
// routines.  Arguments are already in $16-$20/$f16 and the callee-saved mask
// covers everything the standard C ABI requires, so no CALLSEQ is needed.
// The argument registers are not listed here: the node is variadic and carries
// the ones a given call actually sets, so naming a fixed set would both repeat
// those and claim registers the call never wrote.
// Size is 4 (jsr) + 8 (the ldgp expansion, see emitLdgp).
let isCall = 1, Defs = [R26, R29], isCodeGenOnly = 1, Size = 12 in
def OTS_CALL : MbrForm<0b01, (outs), (ins),
                       "jsr $$26, ($$27)\n\tldgp $$29, 0($$26)",
                       [(AlphaOtsCall)]> {
  let Ra = 26;
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
// The single-precision forms move the low 32 bits between an integer register
// and an S_floating register.  i32 is not a legal register type, so custom
// bitcast lowering produces these target nodes (the low 32 bits of an i64 for
// MOVi2f_S; an i64 holding the sign-extended longword for MOVf2i_S).
def MOVi2f_S : AlphaPseudo<(outs FPRC:$dst), (ins GPRC:$src), "# MOVi2f_S",
                           [(set f32:$dst, (AlphaMovi2fS i64:$src))]>;
def MOVf2i_S : AlphaPseudo<(outs GPRC:$dst), (ins FPRC:$src), "# MOVf2i_S",
                           [(set i64:$dst, (AlphaMovf2iS f32:$src))]>;
}

// The FIX extension (ev6) moves the raw bits directly between an integer and a
// floating register, so the MOVi2f/MOVf2i custom inserter emits these instead of
// the stack round trip.  itoft/ftoit move a T_floating (64-bit) value; itofs/
// ftois move an S_floating (low 32 bits).
let Predicates = [HasFIX], hasSideEffects = 0 in {
def ITOFT : FForm<0x14, 0x024, (outs FPRC:$Rc), (ins GPRC:$Ra),
                  "itoft $Ra, $Rc", []> { let Rb = 31; }
def FTOIT : FForm<0x1c, 0x070, (outs GPRC:$Rc), (ins FPRC:$Ra),
                  "ftoit $Ra, $Rc", []> { let Rb = 31; }
def ITOFS : FForm<0x14, 0x004, (outs FPRC:$Rc), (ins GPRC:$Ra),
                  "itofs $Ra, $Rc", []> { let Rb = 31; }
def FTOIS : FForm<0x1c, 0x078, (outs GPRC:$Rc), (ins FPRC:$Ra),
                  "ftois $Ra, $Rc", []> { let Rb = 31; }
}

// Signed integer/floating conversions: move the bits into a floating-point
// register, then convert.  f32 results/inputs go through f64.
def : Pat<(f64 (sint_to_fp GPRC:$x)), (CVTQT (MOVi2f GPRC:$x))>;
def : Pat<(f32 (sint_to_fp GPRC:$x)), (CVTQS (MOVi2f GPRC:$x))>;
def : Pat<(i64 (fp_to_sint f64:$x)), (MOVf2i (CVTTQ f64:$x))>;
// A float in a register is already in T_floating form, so cvttq converts it
// directly; widening first would only add an instruction (and, without -mieee,
// one that faults on an infinity or a NaN).
def : Pat<(i64 (fp_to_sint f32:$x)), (MOVf2i (CVTTQ f32:$x))>;

// An AND whose mask keeps or clears whole bytes (each byte is 0x00 or 0xFF) is a
// single zapnot, so it needs no materialized constant.  This covers zero-
// extension of the low 1/2/4 bytes (masks 0xff / 0xffff / 0xffffffff) and any
// other byte-granular mask.
def bytemask : PatLeaf<(imm), [{
  uint64_t M = N->getZExtValue();
  for (unsigned I = 0; I != 8; ++I) {
    uint64_t B = (M >> (I * 8)) & 0xFF;
    if (B != 0 && B != 0xFF)
      return false;
  }
  return true;
}]>;
def zapnotmask : SDNodeXForm<imm, [{
  uint64_t M = N->getZExtValue();
  unsigned Keep = 0;
  for (unsigned I = 0; I != 8; ++I)
    if (((M >> (I * 8)) & 0xFF) == 0xFF)
      Keep |= 1u << I;
  return CurDAG->getTargetConstant(Keep, SDLoc(N), MVT::i64);
}]>;
let AddedComplexity = 1 in
def : Pat<(and GPRC:$x, bytemask:$m), (ZAPNOTi GPRC:$x, (zapnotmask $m))>;

// Extract a byte, word, or longword at a constant byte offset.  Shifting right
// by a whole number of bytes and masking is a single ext instruction whose
// literal operand is the byte position.
let hasSideEffects = 0 in {
def EXTBLi : OFormL<0x12, 0x06, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                    "extbl $Ra, $lit, $Rc", []>;
def EXTWLi : OFormL<0x12, 0x16, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                    "extwl $Ra, $lit, $Rc", []>;
def EXTLLi : OFormL<0x12, 0x26, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
                    "extll $Ra, $lit, $Rc", []>;
}
// A right shift by a multiple of 8 bytes (up to 56), giving the byte position.
def byteshift : PatLeaf<(imm), [{
  uint64_t S = N->getZExtValue();
  return S <= 56 && (S & 7) == 0;
}]>;
def bytepos : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue() / 8, SDLoc(N), MVT::i64);
}]>;
let AddedComplexity = 1 in {
def : Pat<(and (srl GPRC:$x, byteshift:$s), 0xff),
          (EXTBLi GPRC:$x, (bytepos $s))>;
def : Pat<(and (srl GPRC:$x, byteshift:$s), 0xffff),
          (EXTWLi GPRC:$x, (bytepos $s))>;
def : Pat<(and (srl GPRC:$x, byteshift:$s), 0xffffffff),
          (EXTLLi GPRC:$x, (bytepos $s))>;
}

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

// A comparison against a small unsigned constant (notably zero) folds the
// constant into the compare instead of materializing it.
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETEQ),  (CMPEQi  GPRC:$a, immUExt8:$c)>;
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETNE),  (XORi (CMPEQi GPRC:$a, immUExt8:$c),
                                                      1)>;
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETLT),  (CMPLTi  GPRC:$a, immUExt8:$c)>;
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETLE),  (CMPLEi  GPRC:$a, immUExt8:$c)>;
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETULT), (CMPULTi GPRC:$a, immUExt8:$c)>;
def : Pat<(setcc GPRC:$a, immUExt8:$c, SETULE), (CMPULEi GPRC:$a, immUExt8:$c)>;

// x > 0 swaps its operands to cmplt 0, x, putting zero in the first (register)
// position where the immediate form cannot hold it; use the zero register.
// (x >= 0 and unsigned x > 0 are canonicalized to a sign-bit test and x != 0
// before selection, so they need no pattern here.)
def : Pat<(setcc GPRC:$a, 0, SETGT), (CMPLT R31, GPRC:$a)>;

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

// A floating-point select whose condition is itself a floating-point comparison
// feeds the comparison result (2.0 for true, 0.0 for false) straight into
// fcmovne, which tests against zero -- no round trip through an integer register.
class FSelPat<ValueType svt, CondCode cc, ValueType cvt, Instruction cmp,
              dag a, dag b>
    : Pat<(svt (select (i64 (setcc cvt:$x, cvt:$y, cc)), svt:$t, svt:$f)),
          (FCMOVNE svt:$f, (cmp a, b), svt:$t)>;

let AddedComplexity = 10 in
foreach svt = [f32, f64] in
foreach cvt = [f32, f64] in {
def : FSelPat<svt, SETOEQ, cvt, CMPTEQ, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETOLT, cvt, CMPTLT, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETOLE, cvt, CMPTLE, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETOGT, cvt, CMPTLT, (cvt $y), (cvt $x)>;
def : FSelPat<svt, SETOGE, cvt, CMPTLE, (cvt $y), (cvt $x)>;
def : FSelPat<svt, SETEQ, cvt, CMPTEQ, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETLT, cvt, CMPTLT, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETLE, cvt, CMPTLE, (cvt $x), (cvt $y)>;
def : FSelPat<svt, SETGT, cvt, CMPTLT, (cvt $y), (cvt $x)>;
def : FSelPat<svt, SETGE, cvt, CMPTLE, (cvt $y), (cvt $x)>;
}

// Otherwise move the 0/1 condition into an FP register (its bits are zero or
// non-zero) and use fcmovne to pick the true value.
def : Pat<(f32 (select GPRC:$c, FPRC:$t, FPRC:$f)),
          (FCMOVNE FPRC:$f, (MOVi2f GPRC:$c), FPRC:$t)>;
def : Pat<(f64 (select GPRC:$c, FPRC:$t, FPRC:$f)),
          (FCMOVNE FPRC:$f, (MOVi2f GPRC:$c), FPRC:$t)>;

// Cheap floating-point constants avoid a constant-pool load: $f31 reads as +0.0,
// and an equal compare of $f31 with itself yields exactly +2.0.  These hold for
// both S_floating and T_floating, whose 0.0 and 2.0 share the same register bits.
def immFP0  : PatLeaf<(fpimm), [{ return N->isExactlyValue(+0.0); }]>;
def immFPn0 : PatLeaf<(fpimm), [{ return N->isExactlyValue(-0.0); }]>;
def immFP2  : PatLeaf<(fpimm), [{ return N->isExactlyValue(+2.0); }]>;
foreach vt = [f32, f64] in {
def : Pat<(vt immFP0),  (vt (CPYS  (vt F31), (vt F31)))>;
def : Pat<(vt immFPn0), (vt (CPYSN (vt F31), (vt F31)))>;
def : Pat<(vt immFP2),  (vt (CMPTEQ (vt F31), (vt F31)))>;
}

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
// lds expands the S_floating value it loads into T_floating form, so the same
// instruction also loads a float straight into a double.
let isCodeGenOnly = 1 in
def LDSf64 : MForm<0x22, (outs FPRC:$Ra), (ins memri:$addr), "lds $Ra, $addr",
                   []>;
}

// Software-directed prefetch: a load whose destination is R31 or F31 brings a
// cache line in without producing a result.  The 21264 HRM gives exactly one
// instruction to each of the three hints it defines: ldl to R31 is a normal
// prefetch (section 2.6.1), lds to F31 asks for the line with modify intent
// (2.6.2), and ldq to R31 asks for it to be evicted next, for data with no
// temporal locality (2.6.3).  There is no encoding that asks for both modify
// intent and evict next, and none of the other loads in 2.6.1's list -- ldt to
// F31 among them -- carries a hint of its own.
let mayLoad = 1, hasSideEffects = 0, Ra = 31 in {
def PREFR   : MForm<0x28, (outs), (ins memri:$addr), "ldl $$31, $addr", []>;
def PREFREN : MForm<0x29, (outs), (ins memri:$addr), "ldq $$31, $addr", []>;
def PREFW   : MForm<0x22, (outs), (ins memri:$addr), "lds $$f31, $addr", []>;
}

// Select the prefetch flavor from the read/write and locality operands of the
// llvm.prefetch intrinsic.  A read with no temporal locality takes the
// evict-next form; a write takes modify intent whatever its locality, since
// asking for eviction instead would give up the intent and fetch the line
// read-shared, leaving the store that follows to upgrade it.  On processors
// before the 21264 no pattern matches and the prefetch is dropped, since a
// load to R31/F31 there can fault.
class PrefFrag<int Write, bit EvictNext>
    : PatFrag<(ops node:$ptr, node:$rw, node:$loc, node:$cache),
              (prefetch node:$ptr, node:$rw, node:$loc, node:$cache), [{
  return N->getConstantOperandVal(2) == }] # Write #
         !if(EvictNext, [{ && N->getConstantOperandVal(3) == 0; }], [{ ; }])>;

let Predicates = [HasPrefetch] in {
def : Pat<(PrefFrag<0, 0> ADDRri:$addr, timm, timm, timm), (PREFR memri:$addr)>;
def : Pat<(PrefFrag<1, 0> ADDRri:$addr, timm, timm, timm), (PREFW memri:$addr)>;
let AddedComplexity = 10 in
def : Pat<(PrefFrag<0, 1> ADDRri:$addr, timm, timm, timm), (PREFREN memri:$addr)>;
}

// An i32 load with don't-care high bits can use the sign-extending ldl; a
// zero-extending load clears the high half with zapnot.
def : Pat<(i64 (extloadi32 ADDRri:$addr)), (LDL memri:$addr)>;
def : Pat<(i64 (zextloadi32 ADDRri:$addr)), (ZAPNOTi (LDL memri:$addr), 15)>;

// An f32 loaded and widened to f64 (e.g. an f32-encoded double constant): lds
// loads the S_floating value and cvtst widens it exactly.
let Predicates = [NotIEEE] in
def : Pat<(f64 (extloadf32 ADDRri:$addr)), (LDSf64 memri:$addr)>;
let Predicates = [HasIEEE] in
def : Pat<(f64 (extloadf32 ADDRri:$addr)), (CVTST_S (LDS memri:$addr))>;

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
let AddedComplexity = 1 in {
def : Pat<(truncstorei8 (i64 0), ADDRri:$addr), (STB R31, memri:$addr)>;
def : Pat<(truncstorei16 (i64 0), ADDRri:$addr), (STW R31, memri:$addr)>;
}
} // Predicates = [HasBWX]

// Pre-BWX byte/word access.  ldq_u loads the aligned quadword containing the
// datum (ignoring the low three address bits); extbl/extwl extract the byte or
// word at the position given by the low address bits, zero-extended.
// ldq_u/stq_u take a base register with an implicit zero displacement; the CPU
// ignores the low three address bits.
let mayLoad = 1, hasSideEffects = 0, isCodeGenOnly = 1 in
def LDQ_U : MFormZ<0x0b, (outs GPRC:$Ra), (ins memzGPR:$base),
                   "ldq_u $Ra, $base", [(set i64:$Ra, (AlphaLdqU i64:$base))]>;
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

// Extract / insert / mask a byte, word, or longword at a variable byte position
// held in a register.  The ext/ins/msk instructions take the position (low 3
// bits) directly, while the shift/mask idiom shifts by that times eight; the two
// agree because 8*n mod 64 is 8*(n mod 8), so a shift by (n << 3) picks the same
// byte the position n does.
let AddedComplexity = 2 in {
def : Pat<(and (srl GPRC:$x, (shl GPRC:$n, (i64 3))), 0xff),
          (EXTBL GPRC:$x, GPRC:$n)>;
def : Pat<(and (srl GPRC:$x, (shl GPRC:$n, (i64 3))), 0xffff),
          (EXTWL GPRC:$x, GPRC:$n)>;
def : Pat<(and (srl GPRC:$x, (shl GPRC:$n, (i64 3))), 0xffffffff),
          (EXTLL GPRC:$x, GPRC:$n)>;
def : Pat<(shl (and GPRC:$y, 0xff), (shl GPRC:$n, (i64 3))),
          (INSBL GPRC:$y, GPRC:$n)>;
def : Pat<(shl (and GPRC:$y, 0xffff), (shl GPRC:$n, (i64 3))),
          (INSWL GPRC:$y, GPRC:$n)>;
def : Pat<(shl (and GPRC:$y, 0xffffffff), (shl GPRC:$n, (i64 3))),
          (INSLL GPRC:$y, GPRC:$n)>;
def : Pat<(and GPRC:$x, (not (shl (i64 0xff), (shl GPRC:$n, (i64 3))))),
          (MSKBL GPRC:$x, GPRC:$n)>;
def : Pat<(and GPRC:$x, (not (shl (i64 0xffff), (shl GPRC:$n, (i64 3))))),
          (MSKWL GPRC:$x, GPRC:$n)>;
def : Pat<(and GPRC:$x, (not (shl (i64 0xffffffff), (shl GPRC:$n, (i64 3))))),
          (MSKLL GPRC:$x, GPRC:$n)>;
}

let mayStore = 1, hasSideEffects = 0, isCodeGenOnly = 1 in
def STQ_U : MFormZ<0x0f, (outs), (ins GPRC:$Ra, memzGPR:$base),
                   "stq_u $Ra, $base", [(AlphaStqU i64:$Ra, i64:$base)]>;
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

// A misaligned store updates the one or two quadwords the field falls in, and
// has to stay indivisible for the same reason the byte store does: two of them
// can land in one quadword, and interleaving the reads with the writes loses
// whichever field was written first.  The four scratch registers are outputs so
// the allocator provides them.
// Eleven instructions after expansion; see RMW_STOREI8 above for why the size
// has to say so before the expansion happens.
let mayLoad = 1, mayStore = 1, hasSideEffects = 0, Size = 44,
    Constraints = "@earlyclobber $t1,@earlyclobber $t2,@earlyclobber $t3,"
                  "@earlyclobber $t4" in
def RMW_USTORE : AlphaPseudo<(outs GPRC:$t1, GPRC:$t2, GPRC:$t3, GPRC:$t4),
                             (ins GPRC:$val, GPRC:$addr, i64imm:$bytes), "",
                             []>;

// Lock-based byte/word store: the read-modify-write is done with an
// ldq_l/stq_c loop by a custom inserter.  Used for -msafe-bwa and, always, for
// pre-BWX atomic stores (where the plain read-modify-write is not atomic).
let usesCustomInserter = 1, mayLoad = 1, mayStore = 1 in {
def SAFE_STOREI8  : AlphaPseudo<(outs), (ins GPRC:$val, GPRC:$addr), "", []>;
def SAFE_STOREI16 : AlphaPseudo<(outs), (ins GPRC:$val, GPRC:$addr), "", []>;
// -msafe-partial misaligned store: the byte width picks the insert/mask forms.
def SAFE_USTORE : AlphaPseudo<(outs), (ins GPRC:$val, GPRC:$addr, i64imm:$bytes),
                              "", [(AlphaSafeUStore GPRC:$val, GPRC:$addr,
                                                    imm:$bytes)]>;
}
let Predicates = [SafeBWStore] in {
def : Pat<(truncstorei8 GPRC:$val, ADDRr:$addr),
          (SAFE_STOREI8 GPRC:$val, GPRC:$addr)>;
def : Pat<(truncstorei16 GPRC:$val, ADDRr:$addr),
          (SAFE_STOREI16 GPRC:$val, GPRC:$addr)>;
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

// Storing a zero reads the zero register directly instead of materializing 0
// into a temporary (common when zeroing memory).
let AddedComplexity = 1 in {
def : Pat<(store (i64 0), ADDRri:$addr), (STQ R31, memri:$addr)>;
def : Pat<(truncstorei32 (i64 0), ADDRri:$addr), (STL R31, memri:$addr)>;
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

// A naturally aligned byte or word load is atomic too: the aligned quadword it
// sits in is read atomically and the field extracted.
let Predicates = [HasBWX] in {
def : Pat<(i64 (atomic_load_azext_8 ADDRri:$addr)), (LDBU memri:$addr)>;
def : Pat<(i64 (atomic_load_azext_16 ADDRri:$addr)), (LDWU memri:$addr)>;
def : Pat<(i64 (atomic_load_sext_8 ADDRri:$addr)), (SEXTB (LDBU memri:$addr))>;
def : Pat<(i64 (atomic_load_sext_16 ADDRri:$addr)), (SEXTW (LDWU memri:$addr))>;
}
let Predicates = [NotBWX] in {
def : Pat<(i64 (atomic_load_azext_8 ADDRr:$a)),
          (EXTBL (LDQ_U GPRC:$a), GPRC:$a)>;
def : Pat<(i64 (atomic_load_azext_16 ADDRr:$a)),
          (EXTWL (LDQ_U GPRC:$a), GPRC:$a)>;
def : Pat<(i64 (atomic_load_sext_8 ADDRr:$a)),
          (SRAi (SLLi (EXTBL (LDQ_U GPRC:$a), GPRC:$a), 56), 56)>;
def : Pat<(i64 (atomic_load_sext_16 ADDRr:$a)),
          (SRAi (SLLi (EXTWL (LDQ_U GPRC:$a), GPRC:$a), 48), 48)>;
}

// A byte/word atomic store: stb/stw with BWX; otherwise the lock-based
// ldq_l/stq_c sequence, since the plain read-modify-write is not atomic.
let Predicates = [HasBWX] in {
def : Pat<(atomic_store_8 GPRC:$val, ADDRri:$addr),
          (STB GPRC:$val, memri:$addr)>;
def : Pat<(atomic_store_16 GPRC:$val, ADDRri:$addr),
          (STW GPRC:$val, memri:$addr)>;
}
let Predicates = [NotBWX] in {
def : Pat<(atomic_store_8 GPRC:$val, ADDRr:$addr),
          (SAFE_STOREI8 GPRC:$val, GPRC:$addr)>;
def : Pat<(atomic_store_16 GPRC:$val, ADDRr:$addr),
          (SAFE_STOREI16 GPRC:$val, GPRC:$addr)>;
}

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
  // Replaced by the ldl_l/ldq_l retry loop during selection, so no scheduler
  // ever sees one.
  let hasNoSchedulingInfo = 1;
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
  // Replaced by the ldl_l/ldq_l retry loop during selection, so no scheduler
  // ever sees one.
  let hasNoSchedulingInfo = 1;
}
class AtomicCASvt<ValueType vt>
    : PatFrag<(ops node:$ptr, node:$cmp, node:$new),
              (atomic_cmp_swap node:$ptr, node:$cmp, node:$new)> {
  let IsAtomic = 1;
  let MemoryVT = vt;
}
def ATOMIC_CMPXCHG_I64 : AtomicCAS<AtomicCASvt<i64>>;
def ATOMIC_CMPXCHG_I32 : AtomicCAS<AtomicCASvt<i32>>;
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

// The two aligned addresses and both positioned halves, then one lock loop per
// quadword the field falls in, the second guarded by whether there are two.
let Size = 68,
    Constraints = "@earlyclobber $t0,@earlyclobber $t1,@earlyclobber $t2,"
                  "@earlyclobber $t3,@earlyclobber $t4,@earlyclobber $t5" in
def SAFE_USTORE_LOOP
    : AlphaPseudo<(outs GPRC:$t0, GPRC:$t1, GPRC:$t2, GPRC:$t3, GPRC:$t4,
                        GPRC:$t5),
                  (ins GPRC:$val, GPRC:$addr, i64imm:$bytes), "", []>;
}

// Subroutine return: the canonical form `ret $31, ($26), 1` is printed as the
// bare `ret` (matching gas), which also lets the assembler parse it.  $26 is
// not listed as a use: the epilogue reloads it as a frame-destroy instruction
// (so it survives), and leaving it off keeps the return address, which is only
// materialized in the encoding, from reading as undefined before the epilogue
// is inserted (mirrors RISCV's PseudoRET).
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
  // OPERAND_PCREL is what has the printer given the instruction's address, so
  // a disassembly can name where the branch goes rather than by how much.
  let OperandType = "OPERAND_PCREL";
  let PrintMethod = "printBranchTarget";
}

// Unconditional branch: `br $31, target` ($31 discards the saved PC).
let isBranch = 1, isTerminator = 1, isBarrier = 1, Ra = 31 in
def BR : BForm<0x30, (outs), (ins brtarget:$target), "br $$31, $target",
               [(br bb:$target)]>;

// PC-relative subroutine branch: `bsr $23, target` saves the return address in
// $23 (a caller-saved temporary) and branches.  Used by the machine outliner to
// call an outlined function without loading its address through the GOT; the
// outlined function returns with `jmp $31, ($23)`.
let isCall = 1, Ra = 23, Defs = [R23], isCodeGenOnly = 1 in
def BSR : BForm<0x34, (outs), (ins brtarget:$target), "bsr $$23, $target", []>;

// A direct call under -msmall-text: `bsr $26, callee` reaches the callee with a
// PC-relative branch, leaving the return address in $26.  It loads no procedure
// value and does not reload the global pointer afterwards (so $29 is not
// clobbered), because the callee runs on the caller's.  The R_ALPHA_BRSGP
// relocation is what makes that true: the linker aims the branch past the
// callee's own gp prologue, since the caller's gp already serves.  There is no
// stub for the case where the two do not share one -- bfd's elf64-alpha.c
// reports `change in gp: BRSGP' and fails the link -- which is why this form is
// only used where the caller knows the callee is in the same gp group.
def bsrcalltarget : Operand<i64> {
  let EncoderMethod = "getSameGpTargetEncoding";
}
let isCall = 1, Ra = 26, Defs = [R26], isCodeGenOnly = 1 in
def CALLbsr : BForm<0x34, (outs), (ins bsrcalltarget:$target),
                    "bsr $$26, $target\t\t!samegp", []>;
def : Pat<(AlphaCallBsr tglobaladdr:$t), (CALLbsr tglobaladdr:$t)>;
def : Pat<(AlphaCallBsr texternalsym:$t), (CALLbsr texternalsym:$t)>;

// Conditional branches test a register against zero.  The relational forms
// carry a signed interpretation of the register value.
let isBranch = 1, isTerminator = 1 in {
def BEQ : BForm<0x39, (outs), (ins GPRC:$Ra, brtarget:$target),
                "beq $Ra, $target", [(AlphaBrEQ GPRC:$Ra, bb:$target)]>;
def BNE : BForm<0x3d, (outs), (ins GPRC:$Ra, brtarget:$target),
                "bne $Ra, $target", [(brcond GPRC:$Ra, bb:$target)]>;
def BLT : BForm<0x3a, (outs), (ins GPRC:$Ra, brtarget:$target),
                "blt $Ra, $target", [(AlphaBrLT GPRC:$Ra, bb:$target)]>;
def BLE : BForm<0x3b, (outs), (ins GPRC:$Ra, brtarget:$target),
                "ble $Ra, $target", [(AlphaBrLE GPRC:$Ra, bb:$target)]>;
def BGE : BForm<0x3e, (outs), (ins GPRC:$Ra, brtarget:$target),
                "bge $Ra, $target", [(AlphaBrGE GPRC:$Ra, bb:$target)]>;
def BGT : BForm<0x3f, (outs), (ins GPRC:$Ra, brtarget:$target),
                "bgt $Ra, $target", [(AlphaBrGT GPRC:$Ra, bb:$target)]>;
def BLBC : BForm<0x38, (outs), (ins GPRC:$Ra, brtarget:$target),
                 "blbc $Ra, $target", []>;
def BLBS : BForm<0x3c, (outs), (ins GPRC:$Ra, brtarget:$target),
                 "blbs $Ra, $target", []>;
}
// The floating-point branches test $Fa against zero.  The code generator
// materializes a compare result and tests it in an integer register, so these
// exist for hand-written assembly; a two-instruction cmpt/fb pair is a common
// idiom there, as is counting a loop down in the float unit to keep the
// integer unit free.
let isBranch = 1, isTerminator = 1 in {
class FBForm<bits<6> opc, string mnem>
    : BForm<opc, (outs), (ins FPRC:$Ra, brtarget:$target),
            mnem # " $Ra, $target", []>;
def FBEQ : FBForm<0x31, "fbeq">;
def FBLT : FBForm<0x32, "fblt">;
def FBLE : FBForm<0x33, "fble">;
def FBNE : FBForm<0x35, "fbne">;
def FBGE : FBForm<0x36, "fbge">;
def FBGT : FBForm<0x37, "fbgt">;
}

// A branch on a floating-point comparison tests the comparison result itself,
// which is 2.0 or 0.0, rather than materializing a 0/1 boolean to test in an
// integer register: that costs an ftoit and a shift with the FIX extension, and
// a round trip through the stack without it.  BR_CC is expanded for f32 and
// f64, so the condition always reaches selection as a BRCOND of a SETCC, either
// directly or inverted with an xor when the branch was reversed to fall
// through; the inverted form picks fbeq over fbne and needs no xor of its own.
class FBrPat<CondCode cc, ValueType vt, Instruction cmp, dag a, dag b>
    : Pat<(brcond (i64 (setcc vt:$x, vt:$y, cc)), bb:$target),
          (FBNE (cmp a, b), bb:$target)>;
class FBrNotPat<CondCode cc, ValueType vt, Instruction cmp, dag a, dag b>
    : Pat<(brcond (i64 (xor (setcc vt:$x, vt:$y, cc), 1)), bb:$target),
          (FBEQ (cmp a, b), bb:$target)>;

let AddedComplexity = 10 in
foreach vt = [f32, f64] in {
def : FBrPat<SETOEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FBrPat<SETOLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FBrPat<SETOLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FBrPat<SETOGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FBrPat<SETOGE, vt, CMPTLE, (vt $y), (vt $x)>;
def : FBrPat<SETEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FBrPat<SETLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FBrPat<SETLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FBrPat<SETGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FBrPat<SETGE, vt, CMPTLE, (vt $y), (vt $x)>;

def : FBrNotPat<SETOEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FBrNotPat<SETOLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FBrNotPat<SETOLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FBrNotPat<SETOGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FBrNotPat<SETOGE, vt, CMPTLE, (vt $y), (vt $x)>;
def : FBrNotPat<SETEQ, vt, CMPTEQ, (vt $x), (vt $y)>;
def : FBrNotPat<SETLT, vt, CMPTLT, (vt $x), (vt $y)>;
def : FBrNotPat<SETLE, vt, CMPTLE, (vt $x), (vt $y)>;
def : FBrNotPat<SETGT, vt, CMPTLT, (vt $y), (vt $x)>;
def : FBrNotPat<SETGE, vt, CMPTLE, (vt $y), (vt $x)>;
}

def : Pat<(AlphaBrNE GPRC:$Ra, bb:$target), (BNE GPRC:$Ra, bb:$target)>;
// A low-bit test folds the mask into blbs/blbc (branch on low bit set/clear).
def : Pat<(AlphaBrNE (and GPRC:$Ra, 1), bb:$target),
          (BLBS GPRC:$Ra, bb:$target)>;
def : Pat<(AlphaBrEQ (and GPRC:$Ra, 1), bb:$target),
          (BLBC GPRC:$Ra, bb:$target)>;

// Indirect branch through a register (used for jump tables).
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    Ra = 31 in
def JMP : MbrForm<0b00, (outs), (ins parenGPR:$Rb), "jmp $$31, $Rb, 0", []>;
def : Pat<(brind GPRC:$r), (JMP GPRC:$r)>;

// Tail call: jump to the procedure value in $27 without saving a return
// address, so the callee returns straight to our caller.  The epilogue, which
// tears down the frame and restores $26, is inserted before this terminator.
// isCodeGenOnly, because its encoding is a jmp and the asm matcher and the
// decoder would otherwise have two ways to spell the same word.
let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, Uses = [R27],
    isCodeGenOnly = 1 in
def TCRETURN : MbrForm<0b00, (outs), (ins), "jmp $$31, ($$27), 0",
                       [(AlphaTCReturn)]> {
  let Ra = 31;
  let Rb = 27;
}

// A direct tail call under -msmall-text: the callee is in range, so no procedure
// value is loaded and the jump is a plain PC-relative br.  br writes its return
// address to Ra just as jmp and jsr do; naming $31 there is what discards it,
// which is what makes this a tail call rather than a call.  It takes the same
// !samegp relocation as the call above.
let isTerminator = 1, isReturn = 1, isBarrier = 1, Ra = 31,
    isCodeGenOnly = 1 in
def TCRETURNbr : BForm<0x30, (outs), (ins bsrcalltarget:$target),
                       "br $$31, $target\t\t!samegp", []>;
def : Pat<(AlphaTCReturnBr tglobaladdr:$t), (TCRETURNbr tglobaladdr:$t)>;
def : Pat<(AlphaTCReturnBr texternalsym:$t), (TCRETURNbr texternalsym:$t)>;

//===----------------------------------------------------------------------===//
// GAS-compatible pseudo-instruction aliases.
//===----------------------------------------------------------------------===//

// bis operands are (Rc, Ra, Rb), so mov copies Rb into Rc and clr zeros Rc.
def : InstAlias<"mov $src, $dst", (BIS GPRC:$dst, R31, GPRC:$src)>;
def : InstAlias<"clr $dst", (BIS GPRC:$dst, R31, R31)>;

// or is another name for bis.
def : InstAlias<"or $Ra, $Rb, $Rc", (BIS GPRC:$Rc, GPRC:$Ra, GPRC:$Rb)>;

// Integer negate/sign-extend/complement, each with $31 as the first source.
def : InstAlias<"negq $src, $dst", (SUBQ GPRC:$dst, R31, GPRC:$src)>;
def : InstAlias<"sextl $src, $dst", (ADDL GPRC:$dst, R31, GPRC:$src)>;
def : InstAlias<"not $src, $dst", (ORNOT GPRC:$dst, R31, GPRC:$src)>;

// Floating-point copies built from cpys/cpysn: fmov copies, fneg flips the
// sign, fabs forces it positive with $f31, fnop discards into $f31, fclr
// zeroes by copying $f31 to itself, and negs/negt negate by subtracting from
// $f31.
def : InstAlias<"fmov $src, $dst", (CPYS FPRC:$dst, FPRC:$src, FPRC:$src)>;
def : InstAlias<"fneg $src, $dst", (CPYSN FPRC:$dst, FPRC:$src, FPRC:$src)>;
def : InstAlias<"fabs $src, $dst", (CPYS FPRC:$dst, F31, FPRC:$src)>;
def : InstAlias<"fnop", (CPYS F31, F31, F31)>;
def : InstAlias<"fclr $dst", (CPYS FPRC:$dst, F31, F31)>;
def : InstAlias<"negs $src, $dst", (SUBS FPRC:$dst, F31, FPRC:$src)>;
def : InstAlias<"negt $src, $dst", (SUBT FPRC:$dst, F31, FPRC:$src)>;

// __builtin_alpha_umulh reuses the umulh instruction (also the mulhu pattern).
def : Pat<(int_alpha_umulh i64:$a, i64:$b), (UMULH GPRC:$a, GPRC:$b)>;

// rpcc reads the process cycle counter into Ra (Rb is $31), so it stays
// outside the side-effect-free block above.
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def RPCC : AlphaInst<(outs GPRC:$Ra), (ins), "rpcc $Ra",
                     [(set i64:$Ra, (int_alpha_rpcc))]> {
  bits<5> Ra;
  let Opcode = 0x18;
  let Inst{25-21} = Ra;
  let Inst{20-16} = 31;
  let Inst{15-0} = 0xc000;
}

// Route the byte/word extract, insert and mask intrinsics to the low-form
// instructions that already exist for the pre-BWX access sequences.
def : Pat<(int_alpha_extbl i64:$a, i64:$b), (EXTBL GPRC:$a, GPRC:$b)>;
def : Pat<(int_alpha_extwl i64:$a, i64:$b), (EXTWL GPRC:$a, GPRC:$b)>;
def : Pat<(int_alpha_insbl i64:$a, i64:$b), (INSBL GPRC:$a, GPRC:$b)>;
def : Pat<(int_alpha_inswl i64:$a, i64:$b), (INSWL GPRC:$a, GPRC:$b)>;
def : Pat<(int_alpha_mskbl i64:$a, i64:$b), (MSKBL GPRC:$a, GPRC:$b)>;
def : Pat<(int_alpha_mskwl i64:$a, i64:$b), (MSKWL GPRC:$a, GPRC:$b)>;

// The count intrinsics reuse the CIX instructions (also the generic ctpop/
// ctlz/cttz patterns), so they select only when the cix feature is enabled.
let Predicates = [HasCIX] in {
def : Pat<(int_alpha_ctpop i64:$x), (CTPOP GPRC:$x)>;
def : Pat<(int_alpha_ctlz i64:$x),  (CTLZ GPRC:$x)>;
def : Pat<(int_alpha_cttz i64:$x),  (CTTZ GPRC:$x)>;
}

//===----------------------------------------------------------------------===//
// Additional operand forms accepted by the assembler (GNU as compatibility).
//
// These parse the syntaxes the kernel's hand-written assembly uses.  They carry
// no codegen patterns; the codegen forms above stay isCodeGenOnly where a
// spelling differs.
//===----------------------------------------------------------------------===//

let hasSideEffects = 0 in {

// Byte/word/long/quad extract, insert and mask, plus zap and cmpbge, with a
// literal position (the register forms are defined above; the byte/word low
// ext forms and zapnot already carry their own literal form).
class BLit<bits<6> opc, bits<7> func, string mnem>
    : OFormL<opc, func, (outs GPRC:$Rc), (ins GPRC:$Ra, u8imm:$lit),
             mnem # " $Ra, $lit, $Rc", []>;
def CMPBGEi : BLit<0x10, 0x0f, "cmpbge">;
def ZAPi    : BLit<0x12, 0x30, "zap">;
def EXTQLi  : BLit<0x12, 0x36, "extql">;
def EXTWHi  : BLit<0x12, 0x5a, "extwh">;
def EXTLHi  : BLit<0x12, 0x6a, "extlh">;
def EXTQHi  : BLit<0x12, 0x7a, "extqh">;
def INSBLi  : BLit<0x12, 0x0b, "insbl">;
def INSWLi  : BLit<0x12, 0x1b, "inswl">;
def INSLLi  : BLit<0x12, 0x2b, "insll">;
def INSQLi  : BLit<0x12, 0x3b, "insql">;
def INSWHi  : BLit<0x12, 0x57, "inswh">;
def INSLHi  : BLit<0x12, 0x67, "inslh">;
def INSQHi  : BLit<0x12, 0x77, "insqh">;
def MSKBLi  : BLit<0x12, 0x02, "mskbl">;
def MSKWLi  : BLit<0x12, 0x12, "mskwl">;
def MSKLLi  : BLit<0x12, 0x22, "mskll">;
def MSKQLi  : BLit<0x12, 0x32, "mskql">;
def MSKWHi  : BLit<0x12, 0x52, "mskwh">;
def MSKLHi  : BLit<0x12, 0x62, "msklh">;
def MSKQHi  : BLit<0x12, 0x72, "mskqh">;

// Conditional moves with a literal true-value, cmovXX $Ra, $lit, $Rc.
def CMOVEQi  : BLit<0x11, 0x24, "cmoveq">;
def CMOVNEi  : BLit<0x11, 0x26, "cmovne">;
def CMOVLTi  : BLit<0x11, 0x44, "cmovlt">;
def CMOVGEi  : BLit<0x11, 0x46, "cmovge">;
def CMOVLEi  : BLit<0x11, 0x64, "cmovle">;
def CMOVGTi  : BLit<0x11, 0x66, "cmovgt">;
def CMOVLBSi : BLit<0x11, 0x14, "cmovlbs">;
def CMOVLBCi : BLit<0x11, 0x16, "cmovlbc">;
// The literal forms of the operate instructions that have no pattern of their
// own, and of the ones whose pattern only ever sees a register.  An operate
// instruction takes a literal wherever it takes $Rb, so leaving these out left
// words the assembler could not write and the disassembler could not read.
def ORNOTi  : BLit<0x11, 0x28, "ornot">;
def EQVi    : BLit<0x11, 0x48, "eqv">;
def UMULHi  : BLit<0x13, 0x30, "umulh">;
let Predicates = [HasMVI] in {
def MINSB8i : BLit<0x1c, 0x38, "minsb8">;
def MINSW4i : BLit<0x1c, 0x39, "minsw4">;
def MINUB8i : BLit<0x1c, 0x3a, "minub8">;
def MINUW4i : BLit<0x1c, 0x3b, "minuw4">;
def MAXUB8i : BLit<0x1c, 0x3c, "maxub8">;
def MAXUW4i : BLit<0x1c, 0x3d, "maxuw4">;
def MAXSB8i : BLit<0x1c, 0x3e, "maxsb8">;
def MAXSW4i : BLit<0x1c, 0x3f, "maxsw4">;
}

// amask with a literal mask, amask $lit, $Rc ($Ra is the zero register).
def AMASKi : OFormL<0x11, 0x61, (outs GPRC:$Rc), (ins u8imm:$lit),
                    "amask $lit, $Rc", []> {
  let Ra = 31;
}

// ldq_u/stq_u with the memory operand the assembler writes, disp($base).
let mayLoad = 1 in
def LDQ_Um : MForm<0x0b, (outs GPRC:$Ra), (ins memri:$addr),
                   "ldq_u $Ra, $addr", []>;
let mayStore = 1 in
def STQ_Um : MForm<0x0f, (outs), (ins GPRC:$Ra, memri:$addr),
                   "stq_u $Ra, $addr", []>;

// lda/ldah with a bare displacement (implicit $31 base): materialize a signed
// 16-bit constant, e.g. `lda $0, 128`.
let isAsmParserOnly = 1 in {
def LDAi  : MFormD<0x08, (outs GPRC:$Ra), (ins s16imm:$disp), "lda $Ra, $disp",
                   []> { let Rb = 31; }
def LDAHi : MFormD<0x09, (outs GPRC:$Ra), (ins s16imm:$disp), "ldah $Ra, $disp",
                   []> { let Rb = 31; }
}

// unop: the canonical no-op, ldq_u $31, 0($30).
let isAsmParserOnly = 1 in
def UNOP : AlphaInst<(outs), (ins), "unop", []> {
  let Opcode = 0x0b;
  let Inst{25-21} = 31;
  let Inst{20-16} = 30;
  let Inst{15-0} = 0;
}

// The misc-format cache hints: opcode 0x18 with the address in $Rb and $Ra
// tied off to $31.  They are hints, so dropping one is always legal.
class MiscHint<bits<16> func, string mnem>
    : AlphaInst<(outs), (ins parenGPR:$Rb), mnem # " $Rb", []> {
  bits<5> Rb;
  let Opcode = 0x18;
  let Inst{25-21} = 31;
  let Inst{20-16} = Rb;
  let Inst{15-0} = func;
}

def FETCH   : MiscHint<0x8000, "fetch">;
def FETCH_M : MiscHint<0xa000, "fetch_m">;
def ECB     : MiscHint<0xe800, "ecb">;
def WH64    : MiscHint<0xf800, "wh64">;
def WH64EN  : MiscHint<0xfc00, "wh64en">;
} // hasSideEffects = 0

// bsr with an explicit link register, e.g. the millicode `bsr $23, __divqu`.
let isCall = 1 in
def BSRr : BForm<0x34, (outs), (ins GPRC:$Ra, brtarget:$target),
                 "bsr $Ra, $target", []>;

// br with an explicit link register / a bare target ($31 discards the saved PC).
let isBranch = 1, isTerminator = 1, isBarrier = 1, isAsmParserOnly = 1 in {
def BRr : BForm<0x30, (outs), (ins GPRC:$Ra, brtarget:$target),
                "br $Ra, $target", []>;
let Ra = 31 in
def BRt : BForm<0x30, (outs), (ins brtarget:$target), "br $target", []>;
}

// ret/jmp with the explicit operand forms the assembler writes.  ret defaults
// its hint to 1 (return prediction) unless a link register is given.
let isReturn = 1, isTerminator = 1, isBarrier = 1, isAsmParserOnly = 1 in {
def RETb : MbrForm<0b10, (outs), (ins parenGPR:$Rb), "ret $Rb", []> {
  let Ra = 31;
  let hint = 1;
}
def RETab : MbrForm<0b10, (outs), (ins GPRC:$Ra, parenGPR:$Rb), "ret $Ra, $Rb",
                    []> {
  let hint = 0;
}
// ret takes its hint literally, unlike jsr and jmp, whose operand is a byte
// displacement the assembler divides by four: theirs predicts a target, this
// one selects a prediction-stack action.  GNU as encodes it unscaled.
def RETabh : MbrForm<0b10, (outs), (ins GPRC:$Ra, parenGPR:$Rb, u14imm:$hintval),
                     "ret $Ra, $Rb, $hintval", []> {
  bits<14> hintval;
  let hint = hintval;
}
}
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    isAsmParserOnly = 1 in {
def JMPb : MbrForm<0b00, (outs), (ins parenGPR:$Rb), "jmp $Rb", []> {
  let Ra = 31;
}
def JMPab : MbrForm<0b00, (outs), (ins GPRC:$Ra, parenGPR:$Rb), "jmp $Ra, $Rb",
                    []>;
}

// andnot: an alias of bic (and with the complement of the second operand).
def : InstAlias<"andnot $Ra, $Rb, $Rc", (BIC GPRC:$Rc, GPRC:$Ra, GPRC:$Rb)>;
def : InstAlias<"andnot $Ra, $lit, $Rc", (BICi GPRC:$Rc, GPRC:$Ra, u8imm:$lit)>;

// xornot: the same for eqv (exclusive or with the complement).
def : InstAlias<"xornot $Ra, $Rb, $Rc", (EQV GPRC:$Rc, GPRC:$Ra, GPRC:$Rb)>;
def : InstAlias<"xornot $Ra, $lit, $Rc", (EQVi GPRC:$Rc, GPRC:$Ra, u8imm:$lit)>;

// or: the literal form of the bis alias defined with the other pseudos.
def : InstAlias<"or $Ra, $lit, $Rc", (BISi GPRC:$Rc, GPRC:$Ra, u8imm:$lit)>;
