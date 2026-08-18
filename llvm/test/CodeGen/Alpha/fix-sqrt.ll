; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

declare double @llvm.sqrt.f64(double)
declare float @llvm.sqrt.f32(float)

; CHECK-LABEL: dsqrt:
; CHECK:       sqrtt $f16, $f0
; CHECK:       ret
define double @dsqrt(double %x) {
  %r = call double @llvm.sqrt.f64(double %x)
  ret double %r
}

; CHECK-LABEL: fsqrt:
; CHECK:       sqrts $f16, $f0
; CHECK:       ret
define float @fsqrt(float %x) {
  %r = call float @llvm.sqrt.f32(float %x)
  ret float %r
}
