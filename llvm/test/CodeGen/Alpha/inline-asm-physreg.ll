; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A numbered physical register can be named directly, both as an operand
; constraint and as a global register variable, which is how the kernel binds
; values for its PAL-call and syscall sequences.

; CHECK-LABEL: physreg:
; CHECK: bis $31, $16, $0
define i64 @physreg(i64 %x) {
  %r = call i64 asm "bis $$31, $1, $0", "={$0},{$16}"(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: physreg_fp:
; CHECK: cpys $f16, $f16, $f0
define double @physreg_fp(double %x) {
  %r = call double asm "cpys $1, $1, $0", "={$f0},{$f16}"(double %x)
  ret double %r
}
