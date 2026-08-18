; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; No instruction moves bits between the integer and floating registers on a
; base Alpha, so a bitcast goes through a stack slot: the value is stored in
; the bank that holds it and loaded back in the other one, with no conversion.

; CHECK-LABEL: i2d:
; CHECK:      stq $16, [[OFF:[0-9]+]]($30)
; CHECK-NEXT: ldt $f0, [[OFF]]($30)
; CHECK-NOT:  cvt
define double @i2d(i64 %x) {
  %r = bitcast i64 %x to double
  ret double %r
}

; CHECK-LABEL: d2i:
; CHECK:      stt $f16, [[OFF:[0-9]+]]($30)
; CHECK-NEXT: ldq $0, [[OFF]]($30)
; CHECK-NOT:  cvt
define i64 @d2i(double %x) {
  %r = bitcast double %x to i64
  ret i64 %r
}
