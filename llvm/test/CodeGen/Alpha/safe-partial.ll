; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=UNSAFE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-partial < %s | FileCheck %s --check-prefix=SAFE

; A misaligned store updates the quadword(s) it spans with a read-modify-write.
; By default that is a non-atomic ldq_u/stq_u pair; with -msafe-partial each
; spanned quadword is updated with a lock-based ldq_l/stq_c loop so a concurrent
; access to adjacent bytes is not lost.

; UNSAFE-LABEL: st:
; UNSAFE: ldq_u
; UNSAFE: stq_u
; UNSAFE-NOT: ldq_l

; SAFE-LABEL: st:
; SAFE-NOT: ldq_u
; SAFE: ldq_l
; SAFE: stq_c
; SAFE: beq
; SAFE: ldq_l
; SAFE: stq_c
; SAFE: beq
define void @st(ptr %p, i32 %v) {
  store i32 %v, ptr %p, align 1
  ret void
}
