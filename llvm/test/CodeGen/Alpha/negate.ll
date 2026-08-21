; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; There is no negate instruction; the zero register is the operand instead, so
; a negation is one subq rather than an lda of zero and a subtract.

; CHECK-LABEL: negate:
; CHECK:      subq $31, $16, $0
; CHECK-NEXT: ret
define i64 @negate(i64 %a) {
  %r = sub i64 0, %a
  ret i64 %r
}

; A subtraction from a non-zero constant still has to materialize it.
; CHECK-LABEL: rsub:
; CHECK:      lda $0, 7($31)
; CHECK-NEXT: subq $0, $16, $0
; CHECK-NEXT: ret
define i64 @rsub(i64 %a) {
  %r = sub i64 7, %a
  ret i64 %r
}
