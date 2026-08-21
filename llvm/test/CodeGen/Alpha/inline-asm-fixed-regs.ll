; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The v, a, b and c constraints each name one fixed integer register, matching
; gcc's alpha back end: $0, $24, $25 and $27.

; CHECK-LABEL: v_and_a:
; CHECK: bis $31, $16, $24
; CHECK: bis $31, $24, $0
define i64 @v_and_a(i64 %x) {
  %r = call i64 asm "bis $$31, $1, $0", "=v,a"(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: b_and_c:
; CHECK: bis $31, $27, $25
define i64 @b_and_c(i64 %x) {
  %r = call i64 asm "bis $$31, $1, $0", "=b,c"(i64 %x)
  ret i64 %r
}
