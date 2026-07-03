; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: alloca_roundtrip:
; CHECK:       lda $30, -16($30)
; CHECK:       stq $16, 8($30)
; CHECK:       ldq $0, 8($30)
; CHECK:       lda $30, 16($30)
; CHECK:       ret
define i64 @alloca_roundtrip(i64 %x) {
  %p = alloca i64
  store volatile i64 %x, ptr %p
  %v = load volatile i64, ptr %p
  ret i64 %v
}

; A leaf function that needs no stack has no frame adjustment.
; CHECK-LABEL: leaf:
; CHECK-NOT:   lda $30,
; CHECK:       addq $16, $17, $0
; CHECK:       ret
define i64 @leaf(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}
