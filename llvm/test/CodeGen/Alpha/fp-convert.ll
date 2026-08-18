; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s \
; RUN:   | FileCheck %s --check-prefix=IEEE

; A float in a register is already held in T_floating form, so widening one is a
; register move.  Under -mieee it goes through cvtst, whose /s form is completed
; in software and handles a denormal; the plain form would fault on an infinity
; or a NaN, which is why it is not used otherwise.  GCC makes the same split.
; CHECK-LABEL: ext:
; CHECK:       cpys $f16, $f16, $f0
; CHECK-NEXT:  ret
; IEEE-LABEL:  ext:
; IEEE:        cvtst/s $f16, $f0
; IEEE-NEXT:   ret
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
