//===-- AlphaInstrFormats.td - Alpha instruction formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Alpha instructions are all 32 bits wide.  The architecture defines a handful
// of encoding formats (memory, branch, operate, floating-point operate and
// PALcode), all sharing a 6-bit primary opcode in bits 31-26.  Only the base
// class is provided here; specific formats are added as the instruction set is
// filled in.
//
//===----------------------------------------------------------------------===//

class AlphaInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<32> Inst;
  // A placeholder for fields an instruction does not set, so the disassembler
  // decoder tables can be generated once real encodings exist.
  field bits<32> SoftFail = 0;

  let Namespace = "Alpha";
  let Size = 4;

  bits<6> Opcode = 0;
  let Inst{31-26} = Opcode;

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

// Pseudo-instructions expanded before or during code emission.
class AlphaPseudo<dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}
