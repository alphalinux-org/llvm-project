; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s
; Without the FIX extension there is no square-root instruction, so the
; intrinsic becomes a libcall; the second RUN line takes that path.
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NOFIX

declare double @llvm.sqrt.f64(double)
declare float @llvm.sqrt.f32(float)

; CHECK-LABEL: dsqrt:
; CHECK:       sqrtt $f16, $f0
; NOFIX-LABEL: dsqrt:
; NOFIX:       ldq $27, sqrt($29)
; NOFIX-NOT:   sqrtt
; CHECK:       ret
define double @dsqrt(double %x) {
  %r = call double @llvm.sqrt.f64(double %x)
  ret double %r
}

; CHECK-LABEL: fsqrt:
; CHECK:       sqrts $f16, $f0
; NOFIX-LABEL: fsqrt:
; NOFIX:       ldq $27, sqrtf($29)
; NOFIX-NOT:   sqrts
; CHECK:       ret
define float @fsqrt(float %x) {
  %r = call float @llvm.sqrt.f32(float %x)
  ret float %r
}
