; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

declare double @llvm.floor.f64(double)
declare double @llvm.sin.f64(double)
declare double @llvm.pow.f64(double, double)
declare double @llvm.fma.f64(double, double, double)

; CHECK-LABEL: dfloor:
; CHECK:       ldq $27, floor($29){{.*}}!literal
; CHECK:       jsr $26, ($27)
define double @dfloor(double %x) {
  %r = call double @llvm.floor.f64(double %x)
  ret double %r
}

; CHECK-LABEL: dsin:
; CHECK:       ldq $27, sin($29){{.*}}!literal
define double @dsin(double %x) {
  %r = call double @llvm.sin.f64(double %x)
  ret double %r
}

; CHECK-LABEL: dpow:
; CHECK:       ldq $27, pow($29){{.*}}!literal
define double @dpow(double %a, double %b) {
  %r = call double @llvm.pow.f64(double %a, double %b)
  ret double %r
}

; CHECK-LABEL: dfma:
; CHECK:       ldq $27, fma($29){{.*}}!literal
define double @dfma(double %a, double %b, double %c) {
  %r = call double @llvm.fma.f64(double %a, double %b, double %c)
  ret double %r
}

; The single-precision forms call the f32 routines rather than promoting to
; double and calling the f64 ones, which would round twice.
declare float @llvm.floor.f32(float)
declare float @llvm.sin.f32(float)
declare float @llvm.pow.f32(float, float)
declare float @llvm.fma.f32(float, float, float)

; CHECK-LABEL: ffloor:
; CHECK:       ldq $27, floorf($29){{.*}}!literal
define float @ffloor(float %x) {
  %r = call float @llvm.floor.f32(float %x)
  ret float %r
}

; CHECK-LABEL: fsin:
; CHECK:       ldq $27, sinf($29){{.*}}!literal
define float @fsin(float %x) {
  %r = call float @llvm.sin.f32(float %x)
  ret float %r
}

; CHECK-LABEL: fpow:
; CHECK:       ldq $27, powf($29){{.*}}!literal
define float @fpow(float %a, float %b) {
  %r = call float @llvm.pow.f32(float %a, float %b)
  ret float %r
}

; CHECK-LABEL: ffma:
; CHECK:       ldq $27, fmaf($29){{.*}}!literal
define float @ffma(float %a, float %b, float %c) {
  %r = call float @llvm.fma.f32(float %a, float %b, float %c)
  ret float %r
}
