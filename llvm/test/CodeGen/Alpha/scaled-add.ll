; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; s4addq/s8addq (and the subtract forms) fold a shift-by-2/3 and an add or
; subtract into one instruction, which is how array indexing is addressed.

; CHECK-LABEL: gep8:
; CHECK:       s8addq $17, $16, $0
; CHECK:       ret
define ptr @gep8(ptr %a, i64 %i) {
  %p = getelementptr i64, ptr %a, i64 %i
  ret ptr %p
}

; CHECK-LABEL: s4a:
; CHECK:       s4addq $16, $17, $0
; CHECK:       ret
define i64 @s4a(i64 %a, i64 %b) {
  %s = shl i64 %a, 2
  %r = add i64 %s, %b
  ret i64 %r
}

; CHECK-LABEL: s8s:
; CHECK:       s8subq $16, $17, $0
; CHECK:       ret
define i64 @s8s(i64 %a, i64 %b) {
  %s = shl i64 %a, 3
  %r = sub i64 %s, %b
  ret i64 %r
}

; The quadword subtract by four, which no case above reaches: s8subq is here
; and s4subq is not.
define i64 @s4s(i64 %a, i64 %b) {
; CHECK-LABEL: s4s:
; CHECK:       s4subq $16, $17, $0
; CHECK:       ret
  %s = shl i64 %a, 2
  %r = sub i64 %s, %b
  ret i64 %r
}
