; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s -o %t
; RUN: llvm-readobj -r --symbols %t | FileCheck %s

; A GP-relative relocation computes sym + addend - GP, which a section symbol
; and an addend give just as well as the symbol does.  Keeping the symbol would
; put every constant-pool entry and every block label a jump table refers to
; into the symbol table, where a symbolizer picks one of them over the function
; that contains it.

; CHECK:      R_ALPHA_GPRELHIGH .rodata.cst8
; CHECK-NEXT: R_ALPHA_GPRELLOW .rodata.cst8
; CHECK:      Symbols [
; CHECK-NOT:    Name: .L
; CHECK:      ]

define double @addpi(double %x) {
  %r = fadd double %x, 0x400921FB54442D18
  ret double %r
}
