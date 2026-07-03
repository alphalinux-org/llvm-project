; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A floating-point select uses fcmovne: the false value is placed in the
; destination and overwritten with the true value when the condition is
; non-zero.

; CHECK-LABEL: selcmp:
; CHECK:       cmptlt $f16, $f17, [[C:\$f[0-9]+]]
; CHECK:       fcmovne [[C]], $f18, $f0
; CHECK:       ret
define double @selcmp(double %a, double %b, double %t, double %f) {
  %c = fcmp olt double %a, %b
  %r = select i1 %c, double %t, double %f
  ret double %r
}

; CHECK-LABEL: seli:
; CHECK:       fcmovne {{\$f[0-9]+}}, $f17, $f0
; CHECK:       ret
define double @seli(i64 %c, double %t, double %f) {
  %b = icmp ne i64 %c, 0
  %r = select i1 %b, double %t, double %f
  ret double %r
}
