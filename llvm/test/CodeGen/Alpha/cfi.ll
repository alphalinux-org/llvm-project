; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; The prologue emits DWARF call-frame information so the unwinder can restore
; the caller: the CFA offset after the stack adjustment and the location of the
; saved return address ($26).  The epilogue mirrors this precisely -- each saved
; register is restored and the CFA returns to the stack pointer right at the
; instructions that undo the frame -- so an asynchronous unwind is correct at
; any point.  A function with a frame pointer also records the saved $15 and
; re-anchors the CFA to it.

declare void @g()

; CHECK-LABEL: leaf_caller:
; CHECK:      .cfi_startproc
; CHECK:      .cfi_def_cfa_offset 16
; CHECK:      .cfi_offset $26, -8
; CHECK:      jsr $26, ($27)
; CHECK:      .cfi_restore $26
; CHECK:      .cfi_def_cfa_offset 0
; CHECK:      .cfi_endproc
define void @leaf_caller() {
  call void @g()
  ret void
}

; CHECK-LABEL: with_fp:
; The offsets are the point: an unwinder given the wrong one reads the wrong
; slot, and a rule with no offset checked says nothing about that.
; CHECK:      .cfi_def_cfa_offset 16
; CHECK:      .cfi_offset $26, -8
; CHECK:      .cfi_offset $15, -16
; CHECK:      .cfi_def_cfa_register $15
; The epilogue has to hand the CFA back to $30 before the frame pointer is
; restored, or an asynchronous unwind between the two reads from a register
; that no longer holds the frame.
; CHECK:      .cfi_restore $26
; CHECK:      bis $31, $15, $30
; CHECK-NEXT: .cfi_def_cfa $30, 16
; CHECK:      .cfi_restore $15
; CHECK:      .cfi_def_cfa_offset 0
define void @with_fp(i64 %n) {
  %p = alloca i64, i64 %n
  call void @g()
  ret void
}
