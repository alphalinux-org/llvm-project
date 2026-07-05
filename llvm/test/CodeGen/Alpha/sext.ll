; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: sext32:
; CHECK:       addl $16, $31, $0
; CHECK-NEXT:  ret
define i64 @sext32(i32 %x) {
  %r = sext i32 %x to i64
  ret i64 %r
}

; A 32-bit add whose result is used as a signed i64 is a single addl, which adds
; and sign-extends in one instruction.
; CHECK-LABEL: add32:
; CHECK:       addl $16, $17, $0
; CHECK-NEXT:  ret
define signext i32 @add32(i32 %a, i32 %b) {
  %r = add i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: sub32:
; CHECK: subl $16, $17, $0
; CHECK-NEXT: ret
define signext i32 @sub32(i32 %a, i32 %b) {
  %r = sub i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: mul32:
; CHECK: mull $16, $17, $0
; CHECK-NEXT: ret
define signext i32 @mul32(i32 %a, i32 %b) {
  %r = mul i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: addimm:
; CHECK: addl $16, 5, $0
; CHECK-NEXT: ret
define signext i32 @addimm(i32 %a) {
  %r = add i32 %a, 5
  ret i32 %r
}
