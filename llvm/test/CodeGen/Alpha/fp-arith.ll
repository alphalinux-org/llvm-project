; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: fadd_f32:
; CHECK:       adds $f16, $f17, $f0
; CHECK-NEXT:  ret
define float @fadd_f32(float %x, float %y) {
  %r = fadd float %x, %y
  ret float %r
}

; CHECK-LABEL: fadd_f64:
; CHECK:       addt $f16, $f17, $f0
; CHECK-NEXT:  ret
define double @fadd_f64(double %x, double %y) {
  %r = fadd double %x, %y
  ret double %r
}

; CHECK-LABEL: fsub_f64:
; CHECK:       subt $f16, $f17, $f0
; CHECK-NEXT:  ret
define double @fsub_f64(double %x, double %y) {
  %r = fsub double %x, %y
  ret double %r
}

; CHECK-LABEL: fmul_f32:
; CHECK:       muls $f16, $f17, $f0
; CHECK-NEXT:  ret
define float @fmul_f32(float %x, float %y) {
  %r = fmul float %x, %y
  ret float %r
}

; CHECK-LABEL: fdiv_f64:
; CHECK:       divt $f16, $f17, $f0
; CHECK-NEXT:  ret
define double @fdiv_f64(double %x, double %y) {
  %r = fdiv double %x, %y
  ret double %r
}

; CHECK-LABEL: fsub_f32:
; CHECK:       subs $f16, $f17, $f0
; CHECK-NEXT:  ret
define float @fsub_f32(float %x, float %y) {
  %r = fsub float %x, %y
  ret float %r
}

; CHECK-LABEL: fmul_f64:
; CHECK:       mult $f16, $f17, $f0
; CHECK-NEXT:  ret
define double @fmul_f64(double %x, double %y) {
  %r = fmul double %x, %y
  ret double %r
}

; CHECK-LABEL: fdiv_f32:
; CHECK:       divs $f16, $f17, $f0
; CHECK-NEXT:  ret
define float @fdiv_f32(float %x, float %y) {
  %r = fdiv float %x, %y
  ret float %r
}
