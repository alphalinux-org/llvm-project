; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A conversion happens in a floating register, so the value is moved into or out
; of one first -- through memory, since nothing moves between the banks.  There
; is no unsigned conversion instruction; both directions are built from the
; signed one.

; CHECK-LABEL: s2d:
; CHECK: cvtqt
define double @s2d(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: s2f:
; CHECK: cvtqs
define float @s2f(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; A float in a register is already in T_floating form, so one convert serves
; both widths.
; CHECK-LABEL: d2s:
; CHECK: cvttq/c
define i64 @d2s(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; CHECK-LABEL: f2s:
; CHECK: cvttq/c
define i64 @f2s(float %x) {
  %r = fptosi float %x to i64
  ret i64 %r
}

; CHECK-LABEL: u2d:
; CHECK: subt
define double @u2d(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: d2u:
; CHECK: cmovne
define i64 @d2u(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}
