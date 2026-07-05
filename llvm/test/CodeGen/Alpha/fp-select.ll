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

; An integer condition still takes the fallback path: the 0/1 is moved into a
; floating register and tested there.  This commit keeps that path, so it keeps
; its test.
define double @seli(i64 %c, double %t, double %f) {
; CHECK-LABEL: seli:
; CHECK:      fcmovne
  %b = icmp ne i64 %c, 0
  %r = select i1 %b, double %t, double %f
  ret double %r
}
