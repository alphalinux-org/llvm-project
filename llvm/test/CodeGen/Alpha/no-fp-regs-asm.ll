; RUN: not llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s 2>&1 \
; RUN:   | FileCheck %s

; An "f" constraint has no register class to bind to under -mno-fp-regs, so the
; operand is reported rather than reaching the assertion in SelectionDAGBuilder
; about copying to an illegal type.  GCC says "impossible constraint in 'asm'"
; for the same source.

; CHECK: error: could not allocate output register for constraint 'f'

define double @fadd_asm(double %a, double %b) {
  %r = call double asm "addt $1, $2, $0", "=f,f,f"(double %a, double %b)
  ret double %r
}
