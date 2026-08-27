; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A call through a function pointer.  The callee has to reach $27, which is the
; procedure value the callee loads its own global pointer from; a direct call's
; instruction sets that up itself, which is why only the indirect form needs
; the copy.  Without it the whole function fell back to the SelectionDAG path.

define i64 @ind(ptr %f) {
; CHECK-LABEL: ind:
; CHECK: bis $31, $16, $27
; CHECK-NEXT: jsr $26, ($27)
; CHECK-NEXT: ldgp $29, 0($26)
  %r = call i64 %f()
  ret i64 %r
}

; $27 is not an argument register, so the copy into it and the ones setting up
; the arguments cannot collide, whichever order they end up in.
define i64 @ind_args(ptr %f, i64 %a) {
; CHECK-LABEL: ind_args:
; CHECK-DAG: bis $31, $16, $27
; CHECK-DAG: bis $31, $17, $16
; CHECK: jsr $26, ($27)
  %r = call i64 %f(i64 %a)
  ret i64 %r
}

define double @ind_fp(ptr %f, double %a) {
; CHECK-LABEL: ind_fp:
; CHECK: bis $31, $16, $27
; CHECK: jsr $26, ($27)
  %r = call double %f(double %a)
  ret double %r
}
