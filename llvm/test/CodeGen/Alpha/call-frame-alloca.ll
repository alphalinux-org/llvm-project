; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A call passes its stack arguments at fixed offsets from the stack pointer, so
; the space for them is part of the frame and no call moves the stack pointer.
; A variable-sized allocation moves it, though, so there the outgoing arguments
; have to be carved out around the call -- otherwise they are written on top of
; the allocation the stack pointer now points at.

declare void @g(i64, i64, i64, i64, i64, i64, i64, i64)

; CHECK-LABEL: alloca_and_stack_args:
; The allocation moves $30, and the two stack arguments go below it.
; CHECK:      subq $30, {{\$[0-9]+}}, [[P:\$[0-9]+]]
; CHECK:      bis $31, [[P]], $30
; CHECK:      lda $30, -16($30)
; CHECK:      stq {{\$[0-9]+}}, 8($30)
; CHECK:      stq {{\$[0-9]+}}, 0($30)
; CHECK:      jsr $26, ($27)
; CHECK-NEXT: ldgp $29, 0($26)
; CHECK-NEXT: lda $30, 16($30)
define void @alloca_and_stack_args(i64 %n) {
  %p = alloca i8, i64 %n
  %v = ptrtoint ptr %p to i64
  call void @g(i64 %v, i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7)
  ret void
}
