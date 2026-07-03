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

// Register-register operate: Ra (25-21), Rb (20-16), 0 (15-12), func (11-5),
// Rc (4-0).
class OForm<bits<6> opc, bits<7> func, dag outs, dag ins, string asmstr,
            list<dag> pattern> : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<5> Rb;
  bits<5> Rc;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-16} = Rb;
  let Inst{15-13} = 0;
  let Inst{12} = 0;
  let Inst{11-5} = func;
  let Inst{4-0} = Rc;
}

// Literal operate: Ra (25-21), 8-bit literal (20-13), 1 (12), func (11-5),
// Rc (4-0).
class OFormL<bits<6> opc, bits<7> func, dag outs, dag ins, string asmstr,
             list<dag> pattern> : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<8> lit;
  bits<5> Rc;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-13} = lit;
  let Inst{12} = 1;
  let Inst{11-5} = func;
  let Inst{4-0} = Rc;
}

// Floating-point operate: Fa (25-21), Fb (20-16), func (15-5), Fc (4-0).
class FForm<bits<6> opc, bits<11> func, dag outs, dag ins, string asmstr,
            list<dag> pattern> : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<5> Rb;
  bits<5> Rc;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-16} = Rb;
  let Inst{15-5} = func;
  let Inst{4-0} = Rc;
}

// Memory: Ra (25-21), then a 21-bit base+displacement field encoded by
// getMemEncoding (Rb << 16 | disp16).
class MForm<bits<6> opc, dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<21> addr;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-0} = addr;
}

// Memory format with the destination/source in the Ra field and a base register
// plus displacement written separately (lda/ldah and the relocated variants).
class MFormD<bits<6> opc, dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<5> Rb;
  bits<16> disp;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-16} = Rb;
  let Inst{15-0} = disp;
}

// Memory format with an implicit zero displacement: Ra (25-21), base (20-16),
// and a zero displacement field (ldq_u/stq_u, which ignore the low three bits
// of the address they are given).
class MFormZ<bits<6> opc, dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<5> base;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-16} = base;
  let Inst{15-0} = 0;
}

// Branch: Ra (25-21), 21-bit PC-relative displacement (20-0), encoded by a
// fixup from the branch target.
class BForm<bits<6> opc, dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<21> target;
  let Opcode = opc;
  let Inst{25-21} = Ra;
  let Inst{20-0} = target;
}

// Memory-format branch (jsr/ret/jmp), opcode 0x1a: Ra (25-21, return address),
// Rb (20-16, target), 2-bit sub-opcode (15-14), 14-bit hint (13-0).
class MbrForm<bits<2> sub, dag outs, dag ins, string asmstr, list<dag> pattern>
    : AlphaInst<outs, ins, asmstr, pattern> {
  bits<5> Ra;
  bits<5> Rb;
  bits<14> hint = 0;
  let Opcode = 0x1a;
  let Inst{25-21} = Ra;
  let Inst{20-16} = Rb;
  let Inst{15-14} = sub;
  let Inst{13-0} = hint;
}
