; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A conversion happens in a floating register, so the value is moved into or out
; of one first -- through memory, since nothing moves between the banks.  There
; is no unsigned conversion instruction; both directions are built from the
; signed one.

; CHECK-LABEL: sitofp_double:
; CHECK: cvtqt
define double @sitofp_double(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: sitofp_float:
; CHECK: cvtqs
define float @sitofp_float(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; A float in a register is already in T_floating form, so one convert serves
; both widths.
; CHECK-LABEL: fptosi_double:
; CHECK: cvttq/c
define i64 @fptosi_double(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; CHECK-LABEL: fptosi_float:
; CHECK-NOT: cvtst
; CHECK: cvttq/c
define i64 @fptosi_float(float %x) {
  %r = fptosi float %x to i64
  ret i64 %r
}

; CHECK-LABEL: uitofp_double:
; CHECK: subt
define double @uitofp_double(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: fptoui_double:
; CHECK: cmovne
define i64 @fptoui_double(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}
