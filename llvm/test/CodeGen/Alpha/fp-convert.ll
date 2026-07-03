; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Precision conversions between f32 (S_floating) and f64 (T_floating).

; CHECK-LABEL: ext:
; CHECK:       cvtst $f16, $f0
; CHECK-NEXT:  ret
define double @ext(float %x) {
  %r = fpext float %x to double
  ret double %r
}

; CHECK-LABEL: trunc:
; CHECK:       cvtts $f16, $f0
; CHECK-NEXT:  ret
define float @trunc(double %x) {
  %r = fptrunc double %x to float
  ret float %r
}
