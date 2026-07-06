; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 -mattr=+ieee,+trap-precision-insn < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 -mattr=+ieee < %s | FileCheck %s --check-prefix=NOBAR
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -mattr=+ieee,+trap-precision-insn < %s | FileCheck %s --check-prefix=NOBAR
; With no -mcpu the code has to run on an ev4, so the barriers are emitted.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee,+trap-precision-insn < %s | FileCheck %s

; -mtrap-precision=i follows each trapping FP instruction with a trap barrier,
; but only on processors without precise hardware exceptions: on the 21264 (ev6)
; and later trapb is a no-op and is not emitted.  gcc reaches the same
; conclusion by forcing alpha_tp to ALPHA_TP_PROG for PROCESSOR_EV6
; (alpha_option_override in gcc/config/alpha/alpha.cc).

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
