; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Naturally aligned integer loads and stores are atomic.  The atomic expander
; inserts memory barriers (mb) around the stronger orderings.

; CHECK-LABEL: load_monotonic:
; CHECK:       ldq $0, 0($16)
; CHECK-NEXT:  ret
define i64 @load_monotonic(ptr %p) {
  %v = load atomic i64, ptr %p monotonic, align 8
  ret i64 %v
}

; A sequentially consistent load is fenced on both sides.  The leading mb is
; what keeps an earlier SC store from being reordered past this load: without
; it two threads that each store to one location and then load the other can
; both read the stale value.
; CHECK-LABEL: load_seqcst:
; CHECK:       mb
; CHECK-NEXT:  ldq $0, 0($16)
; CHECK-NEXT:  mb
; CHECK:       ret
define i64 @load_seqcst(ptr %p) {
  %v = load atomic i64, ptr %p seq_cst, align 8
  ret i64 %v
}

; CHECK-LABEL: store_seqcst:
; CHECK:       mb
; CHECK-NEXT:  stq $17, 0($16)
; CHECK-NEXT:  mb
; CHECK:       ret
define void @store_seqcst(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p seq_cst, align 8
  ret void
}

; CHECK-LABEL: load32:
; CHECK:       ldl $0, 0($16)
; CHECK:       ret
define i32 @load32(ptr %p) {
  %v = load atomic i32, ptr %p monotonic, align 4
  ret i32 %v
}
