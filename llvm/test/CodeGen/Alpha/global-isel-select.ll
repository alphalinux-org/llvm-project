; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; cmovne leaves its destination alone when the condition is zero, so a select is
; the false value in the destination and a conditional move of the true one over
; it.

; CHECK-LABEL: sel_i64:
; CHECK: cmovne
define i64 @sel_i64(i1 %c, i64 %a, i64 %b) {
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}

; CHECK-LABEL: sel_ptr:
; CHECK: cmovne
define ptr @sel_ptr(i1 %c, ptr %a, ptr %b) {
  %r = select i1 %c, ptr %a, ptr %b
  ret ptr %r
}

; A narrower select is widened to a whole register.
; CHECK-LABEL: sel_i32:
; CHECK: cmovne
define i32 @sel_i32(i1 %c, i32 %a, i32 %b) {
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; CHECK-LABEL: sel_cmp:
; CHECK: cmovne
define i64 @sel_cmp(i64 %x, i64 %a, i64 %b) {
  %c = icmp sgt i64 %x, 0
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}
