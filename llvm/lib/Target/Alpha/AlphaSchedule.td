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

// A count/MVI source operand.  On the EV6 the count+MVI unit exists only in
// cluster 0 and the integer multiplier only in cluster 1, so a multiply result
// consumed here has to cross clusters and arrives a cycle late.
def Rd_IMVIOp : SchedRead;

//===----------------------------------------------------------------------===//
// Instruction to write-class mapping, instantiated once per model.
//===----------------------------------------------------------------------===//

multiclass AlphaInstRW {
  def : InstRW<[Wr_IALU], (instrs
      ADDL, ADDQ, ADDQi, SUBQ, SUBQi, S4ADDQ, S8ADDQ, S4SUBQ, S8SUBQ,
      LDA, LDAH, LEA, AMASK, IMPLVER, SEXTB, SEXTW)>;
  def : InstRW<[Wr_ILog], (instrs
      AND, ANDi, BIC, BICi, BIS, BISi, ORNOT, ORNOTi, XOR, XORi, EQV, EQVi)>;
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
  def : InstRW<[Wr_IMulH, Rd_IMulOp], (instrs UMULHi)>;
  // Split by arity so the source operands can carry Rd_IMVIOp, which the EV6
  // uses to model the cluster crossing from the multiplier.
  def : InstRW<[Wr_IMVI, Rd_IMVIOp], (instrs
      CTLZ, CTPOP, CTTZ, PKLB, PKWB, UNPKBL, UNPKBW)>;
  def : InstRW<[Wr_IMVI, Rd_IMVIOp, Rd_IMVIOp], (instrs
      PERR,
      MINSB8, MINSW4, MINUB8, MINUW4, MAXSB8, MAXSW4, MAXUB8, MAXUW4)>;
  def : InstRW<[Wr_IMVI, Rd_IMVIOp], (instrs
      MINSB8i, MINSW4i, MINUB8i, MINUW4i, MAXSB8i, MAXSW4i, MAXUB8i,
      MAXUW4i)>;
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
      ADDS, ADDT, SUBS, SUBT, CMPTUN, CMPTEQ, CMPTLE, CMPTLT,
      CVTQS, CVTQT, CVTTQ, CVTTQn, CVTTS, CVTST, CPYS, CPYSN)>;
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
  // The scaled forms likewise; the shift is part of the adder.
  def : InstRW<[Wr_IALU], (instrs
      S4ADDLi, S4ADDQi, S4SUBLi, S4SUBQi, S8ADDLi, S8ADDQi, S8SUBLi, S8SUBQi)>;
  def : InstRW<[Wr_ICmov, ReadDefault, Rd_ITest], (instrs CMOVLBC, CMOVLBS, CMOVGE, CMOVLE)>;
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
  def : InstRW<[Wr_JSR], (instrs JSRtlsgd, JSRtlsldm)>;
  def : InstRW<[Wr_FAdd], (instrs FPEXTST)>;
  def : InstRW<[Wr_Misc], (instrs PREFR, PREFREN, PREFW)>;
  def : InstRW<[Wr_FtoI, Rd_FStData], (instrs FTOIT)>;
  def : InstRW<[Wr_ItoF], (instrs ITOFT)>;
  def : InstRW<[Wr_FtoI, Rd_FStData], (instrs FTOIS, MOVf2i_S)>;
  def : InstRW<[Wr_ItoF], (instrs ITOFS, MOVi2f_S)>;
  def : InstRW<[Wr_IALU], (instrs S4ADDL, S4SUBL, S8ADDL, S8SUBL)>;
  def : InstRW<[Wr_IShift], (instrs EXTBLi, EXTWLi, EXTLLi)>;
  def : InstRW<[Wr_IALU], (instrs ADDLi, SUBL, SUBLi)>;
  // A 32-bit multiply takes the multiplier just as a 64-bit one does; leaving
  // it unmapped made a mull chain schedule as though it were an addq.
  def : InstRW<[Wr_IMul, Rd_IMulOp, Rd_IMulOp], (instrs MULL)>;
  def : InstRW<[Wr_IMul, Rd_IMulOp], (instrs MULLi)>;
  def : InstRW<[Wr_JSR], (instrs TCRETURN)>;
  def : InstRW<[Wr_JSR], (instrs BSR)>;
  def : InstRW<[Wr_ICmp], (instrs CMPEQi, CMPLEi, CMPLTi, CMPULEi, CMPULTi)>;
  def : InstRW<[Wr_Misc], (instrs CALL_PAL)>;
  def : InstRW<[Wr_IALU], (instrs AMASKi, LDAi, LDAHi)>;
  def : InstRW<[Wr_ICmp], (instrs CMPBGEi)>;
  def : InstRW<[Wr_ICmov, ReadDefault, Rd_ITest], (instrs
      CMOVEQi, CMOVNEi, CMOVLTi, CMOVGTi, CMOVGEi, CMOVLEi, CMOVLBCi,
      CMOVLBSi)>;
  def : InstRW<[Wr_IShift], (instrs
      ZAPi, EXTQLi, EXTWHi, EXTLHi, EXTQHi, INSBLi, INSWLi, INSLLi, INSQLi,
      INSWHi, INSLHi, INSQHi, MSKBLi, MSKWLi, MSKLLi, MSKQLi, MSKWHi, MSKLHi,
      MSKQHi)>;
  def : InstRW<[Wr_ILoad], (instrs LDQ_Um, UNOP)>;
  def : InstRW<[Wr_IStore, Rd_StData], (instrs STQ_Um)>;
  def : InstRW<[Wr_Br], (instrs BRr, BRt)>;
  def : InstRW<[Wr_JSR], (instrs BSRr, JMPab, JMPb, RETab, RETb)>;
  def : InstRW<[Wr_Misc], (instrs
      TRAPB, WMB, EXCB, ECB, WH64, WH64EN, FETCH, FETCH_M)>;
  def : InstRW<[Wr_JSR], (instrs CALLbsr, TCRETURNbr)>;
  def : InstRW<[Wr_IStore, Rd_StData], (instrs SAFE_USTORE)>;
  def : InstRW<[Wr_Misc], (instrs WRUNIQ)>;
  def : InstRW<[Wr_JSR], (instrs OTS_CALL)>;
  def : InstRW<[Wr_IALU, Wr_IALU], (instrs LDGPself)>;
  def : InstRW<[Wr_FAdd], (instrs CVTQT_D, CVTST_S, CVTTQ_SVID)>;
  // An FP branch tests an FP register, which changes nothing here.
  def : InstRW<[Wr_Br], (instrs FBEQ, FBNE, FBLT, FBLE, FBGT, FBGE)>;
  def : InstRW<[Wr_JSR], (instrs JSRt)>;
}

//===----------------------------------------------------------------------===//
// 21064 (EV4): dual-issue, in-order.
//===----------------------------------------------------------------------===//

def Alpha21064Model : SchedMachineModel {
  let IssueWidth = 2;
  let MicroOpBufferSize = 0;  // In-order.
  let LoadLatency = 3;
  let MispredictPenalty = 5;
  let CompleteModel = 1;
  // In-order, so schedule after register allocation to hide latencies.
  let PostRAScheduler = 1;
}

let SchedModel = Alpha21064Model in {
  def EV4_Ebox : ProcResource<1>;  // Integer operate.
  def EV4_Abox : ProcResource<1>;  // Load/store.
  def EV4_Fbox : ProcResource<1>;  // Floating-point operate.
  def EV4_Mul  : ProcResource<1>;  // Non-pipelined integer multiplier.
  def EV4_FDiv : ProcResource<1>;  // Non-pipelined FP divider.

  def : WriteRes<Wr_IALU,   [EV4_Ebox]> { let Latency = 1; }
  def : WriteRes<Wr_ILog,   [EV4_Ebox]> { let Latency = 1; }
  def : WriteRes<Wr_ICmp,   [EV4_Ebox]> { let Latency = 2; }
  def : WriteRes<Wr_ICmov,  [EV4_Ebox]> { let Latency = 2; }
  def : WriteRes<Wr_IShift, [EV4_Ebox]> { let Latency = 2; }
  def : WriteRes<Wr_IMul,   [EV4_Ebox, EV4_Mul]> {
    let Latency = 23; let ReleaseAtCycles = [1, 23];
  }
  def : WriteRes<Wr_IMulH,  [EV4_Ebox, EV4_Mul]> {
    let Latency = 23; let ReleaseAtCycles = [1, 23];
  }
  def : WriteRes<Wr_IMVI,   [EV4_Ebox]> { let Latency = 2; }
  def : WriteRes<Wr_ILoad,  [EV4_Abox]> { let Latency = 3; }
  def : WriteRes<Wr_IStore, [EV4_Abox]> { let Latency = 1; }
  def : WriteRes<Wr_FtoI,   [EV4_Abox]> { let Latency = 3; }
  def : WriteRes<Wr_ItoF,   [EV4_Abox]> { let Latency = 3; }
  def : WriteRes<Wr_FAdd,   [EV4_Fbox]> { let Latency = 6; }
  def : WriteRes<Wr_FMul,   [EV4_Fbox]> { let Latency = 6; }
  def : WriteRes<Wr_FCmov,  [EV4_Fbox]> { let Latency = 6; }
  def : WriteRes<Wr_FDivS,  [EV4_Fbox, EV4_FDiv]> {
    let Latency = 34; let ReleaseAtCycles = [1, 34];
  }
  def : WriteRes<Wr_FDivT,  [EV4_Fbox, EV4_FDiv]> {
    let Latency = 63; let ReleaseAtCycles = [1, 63];
  }
  // Square root needs the FIX extension (ev6), so it is never issued here; these
  // keep the model complete.
  def : WriteRes<Wr_FSqrtS, [EV4_Fbox]> { let Latency = 33; }
  def : WriteRes<Wr_FSqrtT, [EV4_Fbox]> { let Latency = 63; }
  def : WriteRes<Wr_FLoad,  [EV4_Abox]> { let Latency = 3; }
  def : WriteRes<Wr_FStore, [EV4_Abox]> { let Latency = 1; }
  def : WriteRes<Wr_Br,     [EV4_Ebox]> { let Latency = 1; }
  def : WriteRes<Wr_JSR,    [EV4_Ebox]> { let Latency = 3; }
  def : WriteRes<Wr_Fpcr,   [EV4_Fbox]> { let Latency = 6; }
  def : WriteRes<Wr_Misc,   [EV4_Abox]> { let Latency = 1; }
  def : ReadAdvance<Rd_FStData, 0>;  // No store bypass distinction on the EV4.
  def : ReadAdvance<Rd_IMVIOp, 0>;   // No MVI on the EV4.
  // A compare forwarded to a branch test operand is one cycle instead of two.
  def : ReadAdvance<Rd_ITest, 1, [Wr_ICmp]>;
  // Store-data bypass: an integer add/logic/shift/compare/cmov result used as
  // store data forwards with an effective latency of zero (the advance is
  // clamped, so the latency-1 and latency-2 producers all reach zero).
  def : ReadAdvance<Rd_StData, 2,
                    [Wr_IALU, Wr_ILog, Wr_IShift, Wr_ICmp, Wr_ICmov]>;
  def : ReadAdvance<Rd_IMulOp, 0>;  // Multiply-operand delay not modeled here.

  defm : AlphaInstRW;
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
  // In-order, so schedule after register allocation to hide latencies.
  let PostRAScheduler = 1;
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
  // The EV5 has a single cluster, so no cross-cluster delay applies.
  def : ReadAdvance<Rd_IMVIOp, 0>;
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
  // An itof result handed straight to an ftoi costs 8 cycles rather than the 4
  // its latency implies -- measured on an EV68AL, where the round trip is 11.1
  // cycles and the ftoi->itof half is 3.  Routing the value through any FP
  // operate avoids it, so the penalty is specific to the direct hand-off.  It
  // is not modeled: Rd_FStData already carries the -2 advance for FP-operate
  // producers above, and TableGen permits only one ReadAdvance per SchedRead,
  // so expressing both would need a separate operand class.
  def : ReadAdvance<Rd_ITest, 0>;   // No branch-test bypass on the EV6.
  def : ReadAdvance<Rd_StData, 0>;  // Out-of-order; no static store-data bypass.

  // Cross-cluster delay.  The clusters {U0,L0} and {U1,L1} hold separate
  // register files resynchronized once a cycle, so a value produced in one and
  // consumed in the other arrives a cycle late.  For instructions that may
  // issue to either cluster the hardware hides this completely -- it places a
  // dependent consumer in its producer's cluster, and a dependent add chain
  // measures 1.008 cycles per link on an EV68AL rather than 2 -- so modeling
  // it there would make the description worse.  It is only forced between the
  // count/MVI unit, which exists solely in U0, and the multiplier, solely in
  // U1.  Measured there at exactly one cycle: a ctpop chain costs 3 per link
  // and a mulq chain 7, while a chain alternating between them costs 24.2
  // cycles per four links against 20 unpenalized and 24 penalized.
  def : ReadAdvance<Rd_IMulOp, -1, [Wr_IMVI]>;
  def : ReadAdvance<Rd_IMVIOp, -1, [Wr_IMul, Wr_IMulH]>;

  defm : AlphaInstRW;
}
