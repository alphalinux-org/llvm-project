; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; i64 select maps to a conditional move: the false value is placed in the
; destination and cmovne overwrites it with the true value when the condition
; register is non-zero.

; CHECK-LABEL: selcmp:
; CHECK:       bis $31, $19, $0
; CHECK:       cmplt $16, $17, $1
; CHECK:       cmovne $1, $18, $0
; CHECK:       ret
define i64 @selcmp(i64 %a, i64 %b, i64 %t, i64 %f) {
  %c = icmp slt i64 %a, %b
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; CHECK-LABEL: seltrunc:
; CHECK:       cmovne
; CHECK:       ret
define i64 @seltrunc(i64 %c, i64 %t, i64 %f) {
  %b = trunc i64 %c to i1
  %r = select i1 %b, i64 %t, i64 %f
  ret i64 %r
}
