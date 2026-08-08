; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee,+trap-precision-insn < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s --check-prefix=NOBAR

; -mtrap-precision=i follows each trapping FP instruction with a trap barrier.

; CHECK: mult/su
; CHECK-NEXT: trapb
; CHECK: addt/su
; CHECK-NEXT: trapb
; NOBAR-NOT: trapb
define double @f(double %a, double %b, double %c) {
  %m = fmul double %a, %b
  %r = fadd double %m, %c
  ret double %r
}
