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
