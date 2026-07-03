; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A read-modify-write that the atomic expander turns into a compare-and-swap
; loop still gets its ordering from the fences the expander hoists out of it
; beforehand, not from the loop.  The retry loop itself is monotonic, so each
; of these takes exactly the barriers its ordering calls for and no more --
; a doubled barrier here would mean the loop was fenced a second time.

; seq_cst: one barrier before the loop and one after.
; CHECK-LABEL: minmax_seq_cst:
; CHECK:       mb
; CHECK-NOT:   mb
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK-NOT:   mb
; CHECK:       mb
; CHECK-NOT:   mb
; CHECK:       ret
define i64 @minmax_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v seq_cst
  ret i64 %r
}

; acquire: nothing before the loop, one barrier after it.
; CHECK-LABEL: minmax_acquire:
; CHECK-NOT:   mb
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK:       mb
; CHECK-NOT:   mb
; CHECK:       ret
define i64 @minmax_acquire(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v acquire
  ret i64 %r
}

; release: one barrier before the loop, none after.
; CHECK-LABEL: minmax_release:
; CHECK:       mb
; CHECK-NOT:   mb
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK-NOT:   mb
; CHECK:       ret
define i64 @minmax_release(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v release
  ret i64 %r
}

; monotonic: no barriers at all.
; CHECK-LABEL: minmax_monotonic:
; CHECK-NOT:   mb
; CHECK:       ret
define i64 @minmax_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw nand ptr %p, i64 %v monotonic
  ret i64 %r
}
