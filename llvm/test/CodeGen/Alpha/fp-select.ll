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

; A select whose false value is +0.0 puts that zero in fcmovne's tied "old
; value" operand.  Materialising it as $f31 itself put a physical register
; there, which TwoAddressInstructionPass cannot rewrite: it asserted
; "cannot make instruction into two-address form", and with it went every
; source containing `isnan(x) ? 0 : x` -- four of compiler-rt's builtins among
; them.  +0.0 is a copy out of $f31 instead, which the coalescer joins away
; wherever the operand is not tied.
define float @sel_zero_false(float %a) {
; CHECK-LABEL: sel_zero_false:
; CHECK:      cmptun $f0, $f0, $f1
; CHECK-NEXT: fcmovne $f1, $f31, $f0
  %cmp = fcmp uno float %a, %a
  %r = select i1 %cmp, float 0.0, float %a
  ret float %r
}

; The same with the zero on the other arm, where it is the value moved in
; rather than the tied one.
define double @sel_zero_true(double %a, double %b) {
; CHECK-LABEL: sel_zero_true:
; CHECK:      cmpteq $f0, $f17, $f1
; CHECK-NEXT: fcmovne $f1, $f31, $f0
  %cmp = fcmp oeq double %a, %b
  %r = select i1 %cmp, double 0.0, double %a
  ret double %r
}

; An integer condition reaches the same tied operand through the other pattern.
define double @seli_zero_false(i64 %c, double %t) {
; CHECK-LABEL: seli_zero_false:
; CHECK:      fcmovne {{\$f[0-9]+}}, $f17, $f0
  %b = trunc i64 %c to i1
  %r = select i1 %b, double %t, double 0.0
  ret double %r
}
