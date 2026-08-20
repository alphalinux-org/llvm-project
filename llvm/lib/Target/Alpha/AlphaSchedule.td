//===-- AlphaSchedule.td - Alpha Scheduling Models ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Scheduling models for the Alpha implementations:
//   * 21064 (EV4) - dual-issue, in order.
//   * 21164 (EV5) - quad-issue, in order.
//   * 21264/21364 (EV6/EV7) - four-way issue, out of order.
//
// Sources: the 21064 and 21164 Hardware Reference Manuals and the Compiler
// Writer's Guide for the 21264/21364.  The instruction-to-class mapping is
// shared; each model supplies its own functional units and latencies.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Scheduling write classes (model-independent).
//===----------------------------------------------------------------------===//

def Wr_IALU   : SchedWrite; // add / lda.
def Wr_ILog   : SchedWrite; // bitwise logic.
def Wr_ICmp   : SchedWrite; // integer compare.
def Wr_ICmov  : SchedWrite; // integer conditional move.
def Wr_IShift : SchedWrite; // shift, zap, extract/insert/mask.
def Wr_IMul   : SchedWrite; // integer multiply (mulq).
def Wr_IMulH  : SchedWrite; // integer multiply high (umulh).
def Wr_IMVI   : SchedWrite; // count and motion-video instructions.
def Wr_ILoad  : SchedWrite; // integer load.
def Wr_IStore : SchedWrite; // integer store.
def Wr_FtoI   : SchedWrite; // floating-to-integer register move.
def Wr_ItoF   : SchedWrite; // integer-to-floating register move.
def Wr_FAdd   : SchedWrite; // FP add / compare / convert.
def Wr_FMul   : SchedWrite; // FP multiply.
def Wr_FCmov  : SchedWrite; // FP conditional move.
def Wr_FDivS  : SchedWrite; // FP divide, single precision (non-pipelined).
def Wr_FDivT  : SchedWrite; // FP divide, double precision (non-pipelined).
def Wr_FSqrtS : SchedWrite; // FP square root, single precision.
def Wr_FSqrtT : SchedWrite; // FP square root, double precision.
def Wr_FLoad  : SchedWrite; // FP load.
def Wr_FStore : SchedWrite; // FP store.
def Wr_Br     : SchedWrite; // integer conditional/unconditional branch.
def Wr_JSR    : SchedWrite; // jump / call / return.
def Wr_Fpcr   : SchedWrite; // move from the FP control register.
def Wr_Misc   : SchedWrite; // rpcc, memory barriers, etc.

// The floating-point value read by a store or a float-to-integer move.  On the
// EV6 the result of an FP operate reaches these consumers two cycles later than
// a normal consumer, modeled with a negative read advance below.
def Rd_FStData : SchedRead;

// The register tested by a conditional branch or conditional move.  On the EV5
// core a logic or compare result forwarded to this operand has an effective
// latency of zero (21164/21164PC HRM), modeled with a read advance below.
def Rd_ITest : SchedRead;

// The value stored by an integer store.  On the EV4 an integer-unit result used
// as store data forwards a cycle or two early (store-data bypass), modeled per
// model below.  Only the data operand carries this read, not the base address.
def Rd_StData : SchedRead;

// An integer-multiply source operand.  The EV5 multiplier cannot take an integer
// bypass, so a fresh integer result reaches it later than a normal consumer
// (21164/21164PC HRM, "additional time before result available to the multiply
// unit"); modeled as a negative read advance on the EV5.
def Rd_IMulOp : SchedRead;

//===----------------------------------------------------------------------===//
// Instruction to write-class mapping, instantiated once per model.
//===----------------------------------------------------------------------===//

multiclass AlphaInstRW {
  def : InstRW<[Wr_IALU], (instrs
      ADDL, ADDQ, ADDQi, SUBQ, SUBQi, S4ADDQ, S8ADDQ, S4SUBQ, S8SUBQ,
      LDA, LDAH, LEA, AMASK, IMPLVER, SEXTB, SEXTW)>;
  def : InstRW<[Wr_ILog], (instrs
      AND, ANDi, BIC, BICi, BIS, BISi, ORNOT, XOR, XORi, EQV)>;
  def : InstRW<[Wr_ICmp], (instrs
      CMPEQ, CMPLE, CMPLT, CMPULE, CMPULT, CMPBGE)>;
  // The condition operand ($Ra, the second source) can be forwarded early.
  def : InstRW<[Wr_ICmov, ReadDefault, Rd_ITest],
               (instrs CMOVEQ, CMOVNE, CMOVLT, CMOVGT)>;
  def : InstRW<[Wr_IShift], (instrs
      SLL, SLLi, SRL, SRLi, SRA, SRAi, ZAP, ZAPNOT, ZAPNOTi,
      EXTBL, EXTWL, EXTLL, EXTQL, EXTWH, EXTLH, EXTQH,
      INSBL, INSWL, INSLL, INSQL, INSWH, INSLH, INSQH,
      MSKBL, MSKWL, MSKLL, MSKQL, MSKWH, MSKLH, MSKQH)>;
  // Both multiply source operands take the multiply-unit bypass penalty.
  def : InstRW<[Wr_IMul, Rd_IMulOp, Rd_IMulOp], (instrs MULQ)>;
  def : InstRW<[Wr_IMul, Rd_IMulOp], (instrs MULQi)>;
  def : InstRW<[Wr_IMulH, Rd_IMulOp, Rd_IMulOp], (instrs UMULH)>;
  def : InstRW<[Wr_IMVI], (instrs
      CTLZ, CTPOP, CTTZ, PERR,
      MINSB8, MINSW4, MINUB8, MINUW4, MAXSB8, MAXSW4, MAXUB8, MAXUW4,
      PKLB, PKWB, UNPKBL, UNPKBW)>;
  def : InstRW<[Wr_ILoad], (instrs
      LDQ, LDL, LDBU, LDWU, LDQ_U, LDL_L, LDQ_L,
      LDQd, LDQg, LDQl, LDQt, LDQtp, LDLd, LDLg, LDLt)>;
  // The stored value (first operand) can arrive via the store-data bypass.
  def : InstRW<[Wr_IStore, Rd_StData], (instrs
      STB, STL, STW, STQ, STQ_U,
      STLd, STLg, STLt, STQd, STQg, STQt, SAFE_STOREI8, SAFE_STOREI16)>;
  def : InstRW<[Wr_IStore], (instrs STL_C, STQ_C)>;
  def : InstRW<[Wr_FLoad], (instrs LDT, LDS, LDTd, LDTg, LDTt, LDSd, LDSg, LDSt)>;
  // A store reads the value in its first operand; on the EV6 an FP-operate
  // result reaches it two cycles later (Rd_FStData).
  def : InstRW<[Wr_FStore, Rd_FStData], (instrs
      STT, STS, STTd, STTg, STTt, STSd, STSg, STSt)>;
  def : InstRW<[Wr_FAdd], (instrs
      ADDS, ADDT, SUBS, SUBT, CMPTEQ, CMPTLE, CMPTLT,
      CVTQS, CVTQT, CVTTQ, CVTTS, CVTST, CPYS, CPYSN)>;
  def : InstRW<[Wr_FMul], (instrs MULS, MULT)>;
  def : InstRW<[Wr_FCmov], (instrs FCMOVNE)>;
  def : InstRW<[Wr_FDivS], (instrs DIVS)>;
  def : InstRW<[Wr_FDivT], (instrs DIVT)>;
  def : InstRW<[Wr_FSqrtS], (instrs SQRTS)>;
  def : InstRW<[Wr_FSqrtT], (instrs SQRTT)>;
  def : InstRW<[Wr_Fpcr], (instrs MF_FPCR, MT_FPCR)>;
  // float-to-integer move reads its FP source with the same EV6 delay.
  def : InstRW<[Wr_FtoI, Rd_FStData], (instrs MOVf2i)>;
  def : InstRW<[Wr_ItoF], (instrs MOVi2f)>;
  def : InstRW<[Wr_Br], (instrs BR)>;
  // A conditional branch tests its first source, which can be forwarded early.
  def : InstRW<[Wr_Br, Rd_ITest],
               (instrs BEQ, BNE, BLT, BLE, BGT, BGE, BLBC, BLBS)>;
  def : InstRW<[Wr_JSR], (instrs JSR, JSRr, JSRd, JSRdl, JMP, RET)>;
  def : InstRW<[Wr_Misc], (instrs RPCC, MB, RDUNIQ)>;
  // The immediate forms cost what their register forms do.
  def : InstRW<[Wr_IALU], (instrs
      LDAd, LDAg, LDAt, LDAHd, LDAHg, LDAHt, LDAHm, LDAtlsgd, LDAtlsldm, NOP)>;
  // cpys and its sign-manipulating forms are add-pipe operations, as are the
  // conversions with a baked-in qualifier.
  def : InstRW<[Wr_FAdd], (instrs FABSS, FABST, FNEGS, FNEGT, FCPYSS, FCPYST)>;
  def : InstRW<[Wr_JSR], (instrs DIVCALL)>;
  // The gp loads expand to an ldah/lda pair.
  def : InstRW<[Wr_IALU, Wr_IALU], (instrs LDGP)>;
  // These expand into a load, a splice and a store held in one bundle, so they
  // occupy the store pipe for at least as long as a store does.
  def : InstRW<[Wr_IStore, Rd_StData],
               (instrs RMW_STOREI8, RMW_STOREI16, RMW_USTORE)>;
  // A register-to-register copy is whatever it lowers to, and is gone before
  // any scheduler sees it; give it the cheapest operate class.
  def : InstRW<[Wr_IALU], (instrs COPY)>;
}

//===----------------------------------------------------------------------===//
// 21164 (EV5): quad-issue, in-order.  Integer pipes E0 and E1 (shift, store,
// and multiply are E0 only); floating-point add pipe FA and multiply pipe FM.
// The 21164PC (PCA56) shares this core and adds the motion-video instructions,
// which issue to E0 with a two-cycle latency (21164PC HRM, Table 2-9); the base
// 21164 has no MVI, so the Wr_IMVI latency below only affects PCA56.
//===----------------------------------------------------------------------===//

def Alpha21164Model : SchedMachineModel {
  let IssueWidth = 4;
  let MicroOpBufferSize = 0;  // In-order.
  let LoadLatency = 2;
  let MispredictPenalty = 5;
  let CompleteModel = 1;
}

let SchedModel = Alpha21164Model in {
  def EV5_E0 : ProcResource<1>;
  def EV5_E1 : ProcResource<1>;
  def EV5_FA : ProcResource<1>;  // FP add / divide.
  def EV5_FM : ProcResource<1>;  // FP multiply.
  def EV5_E  : ProcResGroup<[EV5_E0, EV5_E1]>;  // Either integer pipe.
  def EV5_Mul  : ProcResource<1>;  // Non-pipelined integer multiplier (on E0).
  def EV5_FDiv : ProcResource<1>;  // Non-pipelined FP divider (on FA).

  def : WriteRes<Wr_IALU,   [EV5_E]>  { let Latency = 1; }
  def : WriteRes<Wr_ILog,   [EV5_E]>  { let Latency = 1; }
  def : WriteRes<Wr_ICmp,   [EV5_E]>  { let Latency = 1; }
  def : WriteRes<Wr_ICmov,  [EV5_E]>  { let Latency = 2; }
  def : WriteRes<Wr_IShift, [EV5_E0]> { let Latency = 1; }
  // mulq/umulh occupy E0 for one cycle and the multiplier for the reuse window.
  def : WriteRes<Wr_IMul,   [EV5_E0, EV5_Mul]> {
    let Latency = 12; let ReleaseAtCycles = [1, 8];
  }
  def : WriteRes<Wr_IMulH,  [EV5_E0, EV5_Mul]> {
    let Latency = 14; let ReleaseAtCycles = [1, 8];
  }
  def : WriteRes<Wr_IMVI,   [EV5_E0]> { let Latency = 2; }
  def : WriteRes<Wr_ILoad,  [EV5_E]>  { let Latency = 2; }
  def : WriteRes<Wr_IStore, [EV5_E0]> { let Latency = 1; }
  def : WriteRes<Wr_FtoI,   [EV5_E]>  { let Latency = 2; }
  def : WriteRes<Wr_ItoF,   [EV5_E]>  { let Latency = 2; }
  def : WriteRes<Wr_FAdd,   [EV5_FA]> { let Latency = 4; }
  def : WriteRes<Wr_FMul,   [EV5_FM]> { let Latency = 4; }
  def : WriteRes<Wr_FCmov,  [EV5_FA]> { let Latency = 4; }
  def : WriteRes<Wr_FDivS,  [EV5_FA, EV5_FDiv]> {
    let Latency = 15; let ReleaseAtCycles = [1, 15];
  }
  def : WriteRes<Wr_FDivT,  [EV5_FA, EV5_FDiv]> {
    let Latency = 22; let ReleaseAtCycles = [1, 22];
  }
  // Square root needs the FIX extension (ev6), so it is never issued here.
  def : WriteRes<Wr_FSqrtS, [EV5_FA]> { let Latency = 18; }
  def : WriteRes<Wr_FSqrtT, [EV5_FA]> { let Latency = 30; }
  def : WriteRes<Wr_FLoad,  [EV5_E]>  { let Latency = 2; }
  def : WriteRes<Wr_FStore, [EV5_E0]> { let Latency = 1; }
  def : WriteRes<Wr_Br,     [EV5_E1]> { let Latency = 1; }
  def : WriteRes<Wr_JSR,    [EV5_E1]> { let Latency = 1; }
  def : WriteRes<Wr_Fpcr,   [EV5_FM]> { let Latency = 4; }
  def : WriteRes<Wr_Misc,   [EV5_E0]> { let Latency = 2; }
  def : ReadAdvance<Rd_FStData, 0>;  // No store bypass distinction on the EV5.
  // A logic or compare result forwarded to a branch/cmov test operand has an
  // effective latency of zero (21164/21164PC special bypass).
  def : ReadAdvance<Rd_ITest, 1, [Wr_ILog, Wr_ICmp]>;
  def : ReadAdvance<Rd_StData, 0>;  // No store-data bypass on the EV5.
  // The multiplier cannot take an integer bypass: a fresh add/logic/shift/
  // compare result (latency 1) reaches it at 3 rather than 1.  Loads and
  // motion-video ops (latency 2) also feed it late, modeled with the same delay.
  def : ReadAdvance<Rd_IMulOp, -2,
                    [Wr_IALU, Wr_ILog, Wr_IShift, Wr_ICmp, Wr_ILoad, Wr_IMVI]>;

  defm : AlphaInstRW;
}

//===----------------------------------------------------------------------===//
// 21264/21364 (EV6/EV7): four-way issue, out of order.  The Ebox has four
// integer pipes in two clusters, each with an upper (U) and lower (L)
// subcluster; the Fbox has add and multiply pipes.
//===----------------------------------------------------------------------===//

def Alpha21264Model : SchedMachineModel {
  let IssueWidth = 4;
  let MicroOpBufferSize = 80;
  let LoadLatency = 3;
  let HighLatency = 10;
  // Table 2-1 of the Compiler Writer's Guide gives 7 cycles, but that is
  // explicitly measured only to the fetch stage of the new target and
  // "ignoring any Ibox pipeline stalls or queuing delay".  What a program
  // actually pays, measured on an EV68AL across three branch densities, is
  // about 11 cycles -- which is the figure if-conversion decisions want.
  let MispredictPenalty = 11;
  let CompleteModel = 1;
}

let SchedModel = Alpha21264Model in {
  def EboxU0 : ProcResource<1>;  // Shift/byte, branch, count+MVI.
  def EboxU1 : ProcResource<1>;  // Shift/byte, branch, integer multiply.
  def EboxL0 : ProcResource<1>;  // Load/store address.
  def EboxL1 : ProcResource<1>;  // Load/store address.
  def EboxU  : ProcResGroup<[EboxU0, EboxU1]>;
  def EboxL  : ProcResGroup<[EboxL0, EboxL1]>;
  def Ebox   : ProcResGroup<[EboxU0, EboxU1, EboxL0, EboxL1]>;
  def Fadd : ProcResource<1>;
  def Fmul : ProcResource<1>;

  def : WriteRes<Wr_IALU,   [Ebox]>   { let Latency = 1; }
  def : WriteRes<Wr_ILog,   [Ebox]>   { let Latency = 1; }
  def : WriteRes<Wr_ICmp,   [Ebox]>   { let Latency = 1; }
  // The 21264 cracks a cmov into two dependent operations (Compiler Writer's
  // Guide 2.2.2), so it issues as two micro-ops with a two-cycle latency.
  def : WriteRes<Wr_ICmov,  [Ebox]>   { let Latency = 2; let NumMicroOps = 2; }
  def : WriteRes<Wr_IShift, [EboxU]>  { let Latency = 1; }
  def : WriteRes<Wr_IMul,   [EboxU1]> { let Latency = 7; }
  def : WriteRes<Wr_IMulH,  [EboxU1]> { let Latency = 7; }
  def : WriteRes<Wr_IMVI,   [EboxU0]> { let Latency = 3; }
  def : WriteRes<Wr_ILoad,  [EboxL]>  { let Latency = 3; }
  def : WriteRes<Wr_IStore, [EboxL]>  { let Latency = 1; }
  def : WriteRes<Wr_FtoI,   [EboxL]>  { let Latency = 3; }
  def : WriteRes<Wr_ItoF,   [EboxL]>  { let Latency = 4; }
  def : WriteRes<Wr_FAdd,   [Fadd]>   { let Latency = 4; }
  def : WriteRes<Wr_FMul,   [Fmul]>   { let Latency = 4; }
  // Like the integer cmov, the 21264 cracks an fcmov into two dependent
  // operations that each occupy the FP add pipe (CWG 2.2.2).
  def : WriteRes<Wr_FCmov,  [Fadd]>   { let Latency = 8; let NumMicroOps = 2; }
  def : WriteRes<Wr_FDivS,  [Fadd]>   { let Latency = 12; let ReleaseAtCycles = [9]; }
  def : WriteRes<Wr_FDivT,  [Fadd]>   { let Latency = 15; let ReleaseAtCycles = [12]; }
  def : WriteRes<Wr_FSqrtS, [Fadd]>   { let Latency = 18; let ReleaseAtCycles = [15]; }
  def : WriteRes<Wr_FSqrtT, [Fadd]>   { let Latency = 33; let ReleaseAtCycles = [30]; }
  def : WriteRes<Wr_FLoad,  [EboxL]>  { let Latency = 4; }
  def : WriteRes<Wr_FStore, [EboxL]>  { let Latency = 1; }
  def : WriteRes<Wr_Br,     [EboxU]>  { let Latency = 1; }
  def : WriteRes<Wr_JSR,    [EboxL0]> { let Latency = 3; }
  def : WriteRes<Wr_Fpcr,   [Fmul]>   { let Latency = 4; }
  def : WriteRes<Wr_Misc,   [EboxL1]> { let Latency = 1; }

  // An FP-operate result reaches a store or float-to-integer move two cycles
  // later than a normal consumer (fadd/fmul latency 6, fcmov 10 to fst/ftoi).
  def : ReadAdvance<Rd_FStData, -2, [Wr_FAdd, Wr_FMul, Wr_FCmov]>;
  def : ReadAdvance<Rd_ITest, 0>;   // No branch-test bypass on the EV6.
  def : ReadAdvance<Rd_StData, 0>;  // Out-of-order; no static store-data bypass.
  def : ReadAdvance<Rd_IMulOp, 0>;  // Out-of-order multiplier; no operand delay.

  defm : AlphaInstRW;
}
