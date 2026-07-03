; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: addq:
; CHECK:       addq $16, $17, $0
; CHECK-NEXT:  ret
define i64 @addq(i64 %x, i64 %y) {
  %r = add i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: subq:
; CHECK:       subq $16, $17, $0
; CHECK-NEXT:  ret
define i64 @subq(i64 %x, i64 %y) {
  %r = sub i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: mulq:
; CHECK:       mulq $16, $17, $0
; CHECK-NEXT:  ret
define i64 @mulq(i64 %x, i64 %y) {
  %r = mul i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: and:
; CHECK:       and $16, $17, $0
; CHECK-NEXT:  ret
define i64 @and(i64 %x, i64 %y) {
  %r = and i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: or:
; CHECK:       bis $16, $17, $0
; CHECK-NEXT:  ret
define i64 @or(i64 %x, i64 %y) {
  %r = or i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: xor:
; CHECK:       xor $16, $17, $0
; CHECK-NEXT:  ret
define i64 @xor(i64 %x, i64 %y) {
  %r = xor i64 %x, %y
  ret i64 %r
}
