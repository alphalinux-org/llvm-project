; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
define double @zero() { ret double 0.0 }
define float @zerof() { ret float 0.0 }
define double @two() { ret double 2.0 }
define float @twof() { ret float 2.0 }
; CHECK-LABEL: zero:
; CHECK: cpys $f31, $f31, $f0
; CHECK-LABEL: two:
; CHECK: cmpteq $f31, $f31, $f0
; CHECK-LABEL: negzero:
; CHECK: cpysn $f31, $f31, $f0
define double @negzero() { ret double -0.0 }
