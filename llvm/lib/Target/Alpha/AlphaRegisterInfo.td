//===-- AlphaRegisterInfo.td - Alpha Register defs ---------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Declarations that describe the Alpha register file.  Alpha has 32 64-bit
// integer registers ($0-$31, $31 hardwired to zero) and 32 floating-point
// registers ($f0-$f31, $f31 hardwired to zero).  DWARF numbers integer
// registers 0-31 and floating-point registers 32-63.
//
//===----------------------------------------------------------------------===//

class AlphaReg<bits<16> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding = Enc;
  let Namespace = "Alpha";
  let AltNames = alt;
}

// Integer register, with its ABI name as an alternate.
class GPR<bits<16> Enc, string n, list<string> alt = []> : AlphaReg<Enc, n, alt>;

// Floating-point register.
class FPR<bits<16> Enc, string n> : AlphaReg<Enc, n>;

//===----------------------------------------------------------------------===//
// Integer registers.
//===----------------------------------------------------------------------===//

def R0  : GPR< 0, "$0",  ["$v0"]>,  DwarfRegNum<[0]>;
def R1  : GPR< 1, "$1",  ["$t0"]>,  DwarfRegNum<[1]>;
def R2  : GPR< 2, "$2",  ["$t1"]>,  DwarfRegNum<[2]>;
def R3  : GPR< 3, "$3",  ["$t2"]>,  DwarfRegNum<[3]>;
def R4  : GPR< 4, "$4",  ["$t3"]>,  DwarfRegNum<[4]>;
def R5  : GPR< 5, "$5",  ["$t4"]>,  DwarfRegNum<[5]>;
def R6  : GPR< 6, "$6",  ["$t5"]>,  DwarfRegNum<[6]>;
def R7  : GPR< 7, "$7",  ["$t6"]>,  DwarfRegNum<[7]>;
def R8  : GPR< 8, "$8",  ["$t7"]>,  DwarfRegNum<[8]>;
def R9  : GPR< 9, "$9",  ["$s0"]>,  DwarfRegNum<[9]>;
def R10 : GPR<10, "$10", ["$s1"]>,  DwarfRegNum<[10]>;
def R11 : GPR<11, "$11", ["$s2"]>,  DwarfRegNum<[11]>;
def R12 : GPR<12, "$12", ["$s3"]>,  DwarfRegNum<[12]>;
def R13 : GPR<13, "$13", ["$s4"]>,  DwarfRegNum<[13]>;
def R14 : GPR<14, "$14", ["$s5"]>,  DwarfRegNum<[14]>;
def R15 : GPR<15, "$15", ["$fp", "$s6"]>, DwarfRegNum<[15]>;
def R16 : GPR<16, "$16", ["$a0"]>,  DwarfRegNum<[16]>;
def R17 : GPR<17, "$17", ["$a1"]>,  DwarfRegNum<[17]>;
def R18 : GPR<18, "$18", ["$a2"]>,  DwarfRegNum<[18]>;
def R19 : GPR<19, "$19", ["$a3"]>,  DwarfRegNum<[19]>;
def R20 : GPR<20, "$20", ["$a4"]>,  DwarfRegNum<[20]>;
def R21 : GPR<21, "$21", ["$a5"]>,  DwarfRegNum<[21]>;
def R22 : GPR<22, "$22", ["$t8"]>,  DwarfRegNum<[22]>;
def R23 : GPR<23, "$23", ["$t9"]>,  DwarfRegNum<[23]>;
def R24 : GPR<24, "$24", ["$t10"]>, DwarfRegNum<[24]>;
def R25 : GPR<25, "$25", ["$t11"]>, DwarfRegNum<[25]>;
def R26 : GPR<26, "$26", ["$ra"]>,  DwarfRegNum<[26]>;
def R27 : GPR<27, "$27", ["$pv", "$t12"]>, DwarfRegNum<[27]>;
def R28 : GPR<28, "$28", ["$at"]>,  DwarfRegNum<[28]>;
def R29 : GPR<29, "$29", ["$gp"]>,  DwarfRegNum<[29]>;
def R30 : GPR<30, "$30", ["$sp"]>,  DwarfRegNum<[30]>;
def R31 : GPR<31, "$31", ["$zero"]>, DwarfRegNum<[31]>;

//===----------------------------------------------------------------------===//
// Floating-point registers.
//===----------------------------------------------------------------------===//

foreach i = 0-31 in
  def F#i : FPR<i, "$f"#i>, DwarfRegNum<[!add(i, 32)]>;

//===----------------------------------------------------------------------===//
// Register classes.
//===----------------------------------------------------------------------===//

// General-purpose registers.  $31 (zero) is included so it can be named, but it
// is reserved in AlphaRegisterInfo::getReservedRegs.  Allocation order roughly
// follows the OSF/ELF convention: caller-saved temporaries first, then
// callee-saved, then argument registers.
def GPRC : RegisterClass<"Alpha", [i64], 64, (add
  (sequence "R%u", 0, 8),      // v0, t0-t7
  (sequence "R%u", 22, 25),    // t8-t11
  R27, R28,                    // pv, at
  (sequence "R%u", 9, 14),     // s0-s5
  (sequence "R%u", 16, 21),    // a0-a5
  R26,                         // ra
  R15, R29, R30, R31)>;        // fp, gp, sp, zero (reserved)

// Floating-point registers, holding either f64 (T_floating) or f32
// (S_floating).  $f31 is hardwired zero and reserved.  The wider type is listed
// first because the class takes its register size from it, and a register in
// this class does hold 64 bits: with f32 first the class claimed to be 32 bits
// wide and a double in it did not match its own class.
// The allocation order follows the same convention GPRC's does: caller-saved
// temporaries first, then callee-saved, then the argument registers.  A plain
// sequence would hand out $f2-$f9 before any temporary, so a leaf function that
// needs two floating registers would save and restore both.
def FPRC : RegisterClass<"Alpha", [f64, f32], 64, (add
  F0, F1,                      // return values
  (sequence "F%u", 10, 15),    // caller-saved temporaries
  (sequence "F%u", 22, 30),    // caller-saved temporaries
  (sequence "F%u", 2, 9),      // callee-saved
  (sequence "F%u", 16, 21),    // arguments
  F31)>;                       // zero (reserved)
