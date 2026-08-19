; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A pointer is a quadword, so inttoptr and ptrtoint change nothing but the
; type.  Both the register bank information and the selector have to say so
; explicitly all the same: without the cases, G_INTTOPTR and G_PTRTOINT reach
; the default arm and the function falls back to SelectionDAG.

; CHECK-LABEL: int_to_ptr:
; CHECK:      bis $31, $16, $0
; CHECK-NEXT: ret
define ptr @int_to_ptr(i64 %x) {
  %r = inttoptr i64 %x to ptr
  ret ptr %r
}

; CHECK-LABEL: ptr_to_int:
; CHECK:      bis $31, $16, $0
; CHECK-NEXT: ret
define i64 @ptr_to_int(ptr %x) {
  %r = ptrtoint ptr %x to i64
  ret i64 %r
}

; The cast is free, so the arithmetic that follows it is the whole function.
; CHECK-LABEL: ptr_to_int_add:
; CHECK:      addq $16, 8, $0
; CHECK-NEXT: ret
define i64 @ptr_to_int_add(ptr %x) {
  %r = ptrtoint ptr %x to i64
  %s = add i64 %r, 8
  ret i64 %s
}

; The G_PTR_ADD is not folded into the load's displacement on this path, but
; the cast itself still costs nothing.
; CHECK-LABEL: round_trip:
; CHECK:      addq $16, 16, [[P:\$[0-9]+]]
; CHECK-NEXT: ldq $0, 0([[P]])
define i64 @round_trip(ptr %p) {
  %i = ptrtoint ptr %p to i64
  %j = add i64 %i, 16
  %q = inttoptr i64 %j to ptr
  %v = load i64, ptr %q
  ret i64 %v
}
