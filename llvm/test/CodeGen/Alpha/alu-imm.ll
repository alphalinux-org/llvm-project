; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: addi:
; CHECK:       addq $16, 5, $0
; CHECK-NEXT:  ret
define i64 @addi(i64 %x) {
  %r = add i64 %x, 5
  ret i64 %r
}

; CHECK-LABEL: shli:
; CHECK:       sll $16, 3, $0
; CHECK-NEXT:  ret
define i64 @shli(i64 %x) {
  %r = shl i64 %x, 3
  ret i64 %r
}

; CHECK-LABEL: ori:
; CHECK:       bis $16, 8, $0
; CHECK-NEXT:  ret
define i64 @ori(i64 %x) {
  %r = or i64 %x, 8
  ret i64 %r
}

; CHECK-LABEL: andi:
; CHECK:       and $16, 12, $0
; CHECK-NEXT:  ret
define i64 @andi(i64 %x) {
  %r = and i64 %x, 12
  ret i64 %r
}

; Constants that do not fit in 8 bits are materialized and used in register form.
; CHECK-LABEL: addbig:
; CHECK:       lda $0, 1000($31)
; CHECK:       addq $16, $0, $0
; CHECK-NEXT:  ret
define i64 @addbig(i64 %x) {
  %r = add i64 %x, 1000
  ret i64 %r
}
