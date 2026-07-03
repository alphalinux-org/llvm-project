; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Unsigned integer/floating conversions expand through the signed conversions
; plus a correction, now that constant pools and floating-point comparisons are
; available.  Just check that they select and use the cvt instructions.

; CHECK-LABEL: uitofp:
; CHECK:       subt
; CHECK:       addt
; CHECK:       ret
define double @uitofp(i64 %x) {
  %r = uitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: fptoui:
; CHECK:       cvttq/c
; CHECK:       ret
define i64 @fptoui(double %x) {
  %r = fptoui double %x to i64
  ret i64 %r
}
