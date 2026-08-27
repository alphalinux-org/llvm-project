; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The three floating constants the hardware can produce without a constant-pool
; load: 0.0 from cpys of the zero register, -0.0 from cpysn, and 2.0 from
; cmpteq of the zero register with itself (a true compare leaves 2.0).  Each is
; covered for both f32 and f64, since the patterns are instantiated per type.

; CHECK-LABEL: zero:
; CHECK:       cpys $f31, $f31, $f0
; CHECK-NOT:   lds
; CHECK-NOT:   ldt
define double @zero() { ret double 0.0 }

; CHECK-LABEL: zerof:
; CHECK:       cpys $f31, $f31, $f0
; CHECK-NOT:   lds
define float @zerof() { ret float 0.0 }

; CHECK-LABEL: two:
; CHECK:       cmpteq $f31, $f31, $f0
; CHECK-NOT:   ldt
define double @two() { ret double 2.0 }

; CHECK-LABEL: twof:
; CHECK:       cmpteq $f31, $f31, $f0
; CHECK-NOT:   lds
define float @twof() { ret float 2.0 }

; CHECK-LABEL: negzero:
; CHECK:       cpysn $f31, $f31, $f0
; CHECK-NOT:   ldt
define double @negzero() { ret double -0.0 }

; A zero that feeds an operand is not copied into a register first: $f31 is
; reserved and never written, so the copy coalesces away and the instruction
; reads the zero register in place, as gcc does.  The absence of the copy is
; the whole point of these cases, so each checks for it.

; CHECK-LABEL: cmpzero:
; CHECK-NOT:   cpys $f31, $f31,
; CHECK:       cmptlt $f31, $f16,
define i1 @cmpzero(double %a) {
  %c = fcmp ogt double %a, 0.0
  ret i1 %c
}

; CHECK-LABEL: cmpzerof:
; CHECK-NOT:   cpys $f31, $f31,
; CHECK:       cmptlt $f31, $f16,
define i1 @cmpzerof(float %a) {
  %c = fcmp ogt float %a, 0.0
  ret i1 %c
}

; CHECK-LABEL: addzero:
; CHECK-NOT:   cpys $f31, $f31,
; CHECK:       addt $f16, $f31, $f0
define double @addzero(double %a) {
  %r = fadd double %a, 0.0
  ret double %r
}

; A select whose condition compares against zero keeps both properties: the
; compare reads $f31 and the false arm is the only value copied.
; CHECK-LABEL: selzero:
; CHECK-NOT:   cpys $f31, $f31,
; CHECK:       cmptlt $f31, $f16,
; CHECK:       fcmovne
define double @selzero(double %a, double %t, double %f) {
  %c = fcmp ogt double %a, 0.0
  %r = select i1 %c, double %t, double %f
  ret double %r
}
