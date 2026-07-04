; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; The prologue emits DWARF call-frame information so the unwinder can restore
; the caller: the CFA offset after the stack adjustment and the location of the
; saved return address ($26).  The epilogue mirrors this precisely -- each saved
; register is restored and the CFA returns to the stack pointer right at the
; instructions that undo the frame -- so an asynchronous unwind is correct at
; any point.  A function with a frame pointer also records the saved $15 and
; re-anchors the CFA to it.
;
; Each rule follows the instruction it describes rather than preceding it: an
; unwind between a .cfi_offset and the store it names would read a slot that
; has not been written yet.

declare void @g()
declare void @h(ptr)

; CHECK-LABEL: caller:
; CHECK:      .cfi_startproc
; CHECK:      lda $30, -16($30)
; CHECK-NEXT: .cfi_def_cfa_offset 16
; CHECK:      stq $26, 8($30)
; CHECK-NEXT: .cfi_offset $26, -8
; CHECK:      jsr $26, ($27)
; CHECK:      .cfi_restore $26
; CHECK:      .cfi_def_cfa_offset 0
; CHECK:      .cfi_endproc
define void @caller() {
  call void @g()
  ret void
}

; CHECK-LABEL: with_fp:
; The offsets are the point: an unwinder given the wrong one reads the wrong
; slot, so every rule is checked with its offset.
; CHECK:      lda $30, -16($30)
; CHECK-NEXT: .cfi_def_cfa_offset 16
; CHECK-NEXT: stq $15, 0($30)
; CHECK-NEXT: .cfi_offset $15, -16
; CHECK-NEXT: bis $31, $30, $15
; CHECK-NEXT: .cfi_def_cfa_register $15
; CHECK-NEXT: stq $26, 8($15)
; CHECK-NEXT: .cfi_offset $26, -8
; The epilogue has to hand the CFA back to $30 before the frame pointer is
; restored, or an asynchronous unwind between the two reads from a register
; that no longer holds the frame.  The .cfi_restore rules follow every reload:
; each says the register is back where it started, and until one is reached the
; unwinder reads the save slot, which still holds the value.
; CHECK:      bis $31, $15, $30
; CHECK-NEXT: .cfi_def_cfa $30, 16
; CHECK:      .cfi_restore $15
; CHECK-NEXT: .cfi_restore $26
; CHECK:      .cfi_def_cfa_offset 0
define void @with_fp(i64 %n) {
  %p = alloca i64, i64 %n
  call void @g()
  ret void
}

; A layout that places live-frame code after an epilogue is what CFIFixup is
; enabled for: the no-frame state the first epilogue leaves behind must not
; leak into the block laid out after it.  The branch weights put the early
; return first, so the teardown really is followed by the block that still has
; the frame, and the pass brackets it with .cfi_remember_state and
; .cfi_restore_state.
; CHECK-LABEL: two_epilogues:
; CHECK:      lda $30, -528($30)
; CHECK-NEXT: .cfi_def_cfa_offset 528
; CHECK:      .cfi_offset $26, -16
; CHECK-NEXT: .cfi_remember_state
; CHECK:      .cfi_restore $26
; CHECK-NEXT: lda $30, 528($30)
; CHECK-NEXT: .cfi_def_cfa_offset 0
; CHECK:      ret
; CHECK:      .cfi_restore_state
define void @two_epilogues(i64 %n, ptr %q) {
entry:
  %p = alloca [64 x i64]
  %c = icmp eq i64 %n, 0
  br i1 %c, label %fast, label %slow, !prof !0
fast:
  ret void
slow:
  call void @h(ptr %p)
  store i64 1, ptr %q
  call void @g()
  ret void
}

!0 = !{!"branch_weights", i32 2000, i32 1}
