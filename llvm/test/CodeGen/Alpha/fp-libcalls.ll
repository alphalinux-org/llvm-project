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
