; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; Both ends of a precision conversion are floating values, so the instruction
; maps entirely to the floating bank.

; A float in a register is already in T_floating form, so widening it is a copy.
; CHECK-LABEL: widen:
; CHECK: cpys $f16, $f16, $f0
define double @widen(float %x) {
  %r = fpext float %x to double
  ret double %r
}

; CHECK-LABEL: narrow:
; CHECK: cvtts $f16, $f0
define float @narrow(double %x) {
  %r = fptrunc double %x to float
  ret float %r
}
