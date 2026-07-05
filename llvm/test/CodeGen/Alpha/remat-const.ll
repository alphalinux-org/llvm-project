; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; A constant materialized with lda is rematerializable: rather than keep it in a
; callee-saved register across the calls (which would need a spill and reload),
; the register allocator recomputes it with a fresh lda at each use.

declare void @use(i64)

; CHECK-LABEL: three_uses:
; CHECK:      lda $16, 1234($31)
; CHECK:      lda $16, 1234($31)
; CHECK:      lda $16, 1234($31)
define void @three_uses() {
  call void @use(i64 1234)
  call void @use(i64 1234)
  call void @use(i64 1234)
  ret void
}
