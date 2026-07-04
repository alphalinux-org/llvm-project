; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -O2 < %s | FileCheck %s

; Each processor generation has its own scheduling model (21064/EV4,
; 21164/EV5, 21264/EV6).  All are accepted and drive the machine scheduler;
; independent work is interleaved with the long integer multiply and the
; floating-point divide.

; CHECK-LABEL: imul_hide:
; CHECK-DAG: mulq
; CHECK-DAG: addq $19, $20,
; CHECK: ret
define i64 @imul_hide(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e) {
  %m = mul i64 %a, %b
  %s = add i64 %m, %c
  %t = add i64 %d, %e
  %r = add i64 %s, %t
  ret i64 %r
}

; CHECK-LABEL: fdiv_hide:
; CHECK-DAG: divt
; CHECK-DAG: addt
; CHECK: ret
define double @fdiv_hide(double %a, double %b, double %c, double %d) {
  %q = fdiv double %a, %b
  %s = fadd double %c, %d
  %r = fadd double %q, %s
  ret double %r
}
