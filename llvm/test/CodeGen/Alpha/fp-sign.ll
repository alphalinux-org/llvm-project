; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: fneg_f64:
; CHECK:       cpysn $f16, $f16, $f0
; CHECK-NEXT:  ret
define double @fneg_f64(double %x) {
  %r = fneg double %x
  ret double %r
}

; CHECK-LABEL: fabs_f64:
; CHECK:       cpys $f31, $f16, $f0
; CHECK-NEXT:  ret
define double @fabs_f64(double %x) {
  %r = call double @llvm.fabs.f64(double %x)
  ret double %r
}

; CHECK-LABEL: fabs_f32:
; CHECK:       cpys $f31, $f16, $f0
; CHECK-NEXT:  ret
define float @fabs_f32(float %x) {
  %r = call float @llvm.fabs.f32(float %x)
  ret float %r
}

; copysign(x, y) keeps the magnitude of x and the sign of y.
; CHECK-LABEL: copysign_f64:
; CHECK:       cpys $f17, $f16, $f0
; CHECK-NEXT:  ret
define double @copysign_f64(double %x, double %y) {
  %r = call double @llvm.copysign.f64(double %x, double %y)
  ret double %r
}

; The f32 forms are the same instructions: the sign of a single is in the same
; place as the sign of a double, so nothing narrows.
; CHECK-LABEL: fneg_f32:
; CHECK:       cpysn $f16, $f16, $f0
; CHECK-NEXT:  ret
define float @fneg_f32(float %x) {
  %r = fneg float %x
  ret float %r
}

; CHECK-LABEL: copysign_f32:
; CHECK:       cpys $f17, $f16, $f0
; CHECK-NEXT:  ret
define float @copysign_f32(float %x, float %y) {
  %r = call float @llvm.copysign.f32(float %x, float %y)
  ret float %r
}

declare double @llvm.fabs.f64(double)
declare float @llvm.fabs.f32(float)
declare double @llvm.copysign.f64(double, double)
declare float @llvm.copysign.f32(float, float)
