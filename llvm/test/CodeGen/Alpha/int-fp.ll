; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: sitofp_f64:
; CHECK:       stq $16, {{[0-9]+}}($30)
; CHECK:       ldt $f0, {{[0-9]+}}($30)
; CHECK:       cvtqt $f0, $f0
; CHECK:       ret
define double @sitofp_f64(i64 %x) {
  %r = sitofp i64 %x to double
  ret double %r
}

; CHECK-LABEL: sitofp_f32:
; CHECK:       cvtqs $f0, $f0
; CHECK:       ret
define float @sitofp_f32(i64 %x) {
  %r = sitofp i64 %x to float
  ret float %r
}

; CHECK-LABEL: fptosi_f64:
; CHECK:       cvttq/c $f16, $f0
; CHECK:       stt $f0, {{[0-9]+}}($30)
; CHECK:       ldq $0, {{[0-9]+}}($30)
; CHECK:       ret
define i64 @fptosi_f64(double %x) {
  %r = fptosi double %x to i64
  ret i64 %r
}

; A plain bitcast is just the stack bounce, with no conversion.
; CHECK-LABEL: bitcast:
; CHECK:       stq $16, {{[0-9]+}}($30)
; CHECK:       ldt $f0, {{[0-9]+}}($30)
; CHECK-NOT:   cvt
; CHECK:       ret
define double @bitcast(i64 %x) {
  %r = bitcast i64 %x to double
  ret double %r
}
