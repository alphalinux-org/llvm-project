; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: sext32:
; CHECK:       addl $16, $31, $0
; CHECK-NEXT:  ret
define i64 @sext32(i32 %x) {
  %r = sext i32 %x to i64
  ret i64 %r
}

; The 32-bit add is an addq (low bits are correct); addl then sign-extends.
; CHECK-LABEL: add32:
; CHECK:       addq $16, $17, $0
; CHECK:       addl $0, $31, $0
; CHECK-NEXT:  ret
define signext i32 @add32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}
