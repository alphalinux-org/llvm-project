; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: not llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s \
; RUN:   -o /dev/null 2>&1 | FileCheck %s --check-prefix=NOFP

; A numbered physical register can be named directly, both as an operand
; constraint and as a global register variable, which is how the kernel binds
; values for its PAL-call and syscall sequences.

; CHECK-LABEL: physreg:
; CHECK: bis $31, $16, $0
define i64 @physreg(i64 %x) {
  %r = call i64 asm "bis $$31, $1, $0", "={$0},{$16}"(i64 %x)
  ret i64 %r
}

; Naming one of the floating-point registers is the one thing -mno-fp-regs
; takes away: the whole file is out of the target's register classes there, so
; f64 is not a legal type and there is no class to hand back for {$f0}.  Report
; the constraint as unsatisfiable -- which is what the `f' constraint does too
; -- rather than returning a class and asserting in getCopyToParts on the copy
; to an illegal type.
; CHECK-LABEL: physreg_fp:
; CHECK: cpys $f16, $f16, $f0
; NOFP: error: could not allocate output register for constraint '{$f0}'
define double @physreg_fp(double %x) {
  %r = call double asm "cpys $1, $1, $0", "={$f0},{$f16}"(double %x)
  ret double %r
}
