; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; CHECK-LABEL: q:
; CHECK-NOT:  lda
; CHECK:      stq $31, 0($16)
; CHECK-NEXT: ret
define void @q(ptr %p) {
  store i64 0, ptr %p
  ret void
}

; CHECK-LABEL: l:
; CHECK-NOT:  lda
; CHECK:      stl $31, 0($16)
; CHECK-NEXT: ret
define void @l(ptr %p) {
  store i32 0, ptr %p
  ret void
}

; CHECK-LABEL: w:
; CHECK-NOT:  lda
; CHECK:      stw $31, 0($16)
; CHECK-NEXT: ret
define void @w(ptr %p) {
  store i16 0, ptr %p
  ret void
}

; CHECK-LABEL: b:
; CHECK-NOT:  lda
; CHECK:      stb $31, 0($16)
; CHECK-NEXT: ret
define void @b(ptr %p) {
  store i8 0, ptr %p
  ret void
}

; A non-zero value is still materialized into a register first.
; CHECK-LABEL: nonzero:
; CHECK:      lda $0, 5($31)
; CHECK-NEXT: stq $0, 0($16)
; CHECK-NEXT: ret
define void @nonzero(ptr %p) {
  store i64 5, ptr %p
  ret void
}
