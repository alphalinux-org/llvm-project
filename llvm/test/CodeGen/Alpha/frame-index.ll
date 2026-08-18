; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The address of a stack slot is the frame pointer plus the slot's offset,
; which is an lda: there is no separate instruction for taking an address.

; CHECK-LABEL: fi:
; CHECK: lda $0, {{[0-9]+}}($30)
define ptr @fi() {
  %p = alloca i64
  ret ptr %p
}

; An offset into the slot is folded into the same lda when it fits the
; displacement.
; CHECK-LABEL: fi_off:
; CHECK: lda $0, {{[0-9]+}}($30)
define ptr @fi_off() {
  %a = alloca [4 x i64]
  %p = getelementptr [4 x i64], ptr %a, i64 0, i64 2
  ret ptr %p
}
