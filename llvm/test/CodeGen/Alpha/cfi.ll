; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; The prologue emits DWARF call-frame information so the unwinder can restore
; the caller: the CFA offset after the stack adjustment and the location of the
; saved return address ($26).  A function with a frame pointer also records the
; saved $15 and re-anchors the CFA to it.

declare void @g()

; CHECK-LABEL: caller:
; CHECK:      .cfi_startproc
; CHECK:      .cfi_def_cfa_offset 16
; CHECK:      .cfi_offset $26,
; CHECK:      jsr $26, ($27)
; CHECK:      .cfi_endproc
define void @caller() {
  call void @g()
  ret void
}

; CHECK-LABEL: with_fp:
; CHECK:      .cfi_def_cfa_offset
; CHECK:      .cfi_offset $15,
; CHECK:      .cfi_def_cfa_register $15
; CHECK:      .cfi_offset $26,
define void @with_fp(i64 %n) {
  %p = alloca i64, i64 %n
  call void @g()
  ret void
}
