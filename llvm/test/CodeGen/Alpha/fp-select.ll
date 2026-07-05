; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A floating-point select on a floating-point comparison feeds the compare
; result (2.0 or 0.0) straight into fcmovne, with no integer round trip.
define double @fsel(double %a, double %b, double %c, double %d) {
; CHECK-LABEL: fsel:
; CHECK:      cmptlt $f16, $f17, $f1
; CHECK-NEXT: fcmovne $f1, $f18
; CHECK-NOT:  stt
; CHECK-NOT:  ldq
  %cmp = fcmp olt double %a, %b
  %r = select i1 %cmp, double %c, double %d
  ret double %r
}
