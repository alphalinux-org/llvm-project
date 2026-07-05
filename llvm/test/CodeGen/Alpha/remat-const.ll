; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s --implicit-check-not='{{bis \$31, \$(9|1[0-5]), \$16}}'

; A constant materialized with lda is rematerializable: rather than keep it in a
; callee-saved register across the calls (which would need a spill and reload),
; the register allocator recomputes it with a fresh lda at each use.

declare void @use(i64)

; CHECK-LABEL: three_uses:
; The constant is recomputed at each use rather than kept in a register across
; the calls.  Keeping it would mean holding it in a callee-saved register and
; copying that into the argument register before each call, which is what the
; implicit-check-not on the RUN line forbids.  It is on the RUN line rather than
; here so that it covers the whole function: a copy written before the last lda
; is what a CHECK-NOT after the three would miss.  Forbidding the save itself
; would not do -- a callee-saved register legitimately holds the procedure value
; across these calls wherever the literal load is hoisted out of them.
; CHECK:      lda $16, 1234($31)
; CHECK:      lda $16, 1234($31)
; CHECK:      lda $16, 1234($31)
define void @three_uses() {
  call void @use(i64 1234)
  call void @use(i64 1234)
  call void @use(i64 1234)
  ret void
}
