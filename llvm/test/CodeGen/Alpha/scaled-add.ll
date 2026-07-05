; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Quadword scaled add/subtract, and the longword forms that fold the sign-extend.
define i64 @q4(i64 %a, i64 %b) {
; CHECK-LABEL: q4:
; CHECK: s4addq $16, $17, $0
  %s = shl i64 %a, 2
  %r = add i64 %s, %b
  ret i64 %r
}

; The quadword subtract, which no other case here reaches.
define i64 @q4sub(i64 %a, i64 %b) {
; CHECK-LABEL: q4sub:
; CHECK: s4subq $16, $17, $0
  %s = shl i64 %a, 2
  %r = sub i64 %s, %b
  ret i64 %r
}
define signext i32 @l4(i32 %a, i32 %b) {
; CHECK-LABEL: l4:
; CHECK: s4addl $16, $17, $0
; CHECK-NOT: addl $0, $31
  %s = shl i32 %a, 2
  %r = add i32 %s, %b
  ret i32 %r
}
define signext i32 @l8sub(i32 %a, i32 %b) {
; CHECK-LABEL: l8sub:
; CHECK: s8subl $16, $17, $0
  %s = shl i32 %a, 3
  %r = sub i32 %s, %b
  ret i32 %r
}
