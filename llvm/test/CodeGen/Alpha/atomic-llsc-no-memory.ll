; RUN: llc -O0 -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -O2 -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The architecture only promises forward progress for a load-locked/store-
; conditional pair with no memory access between them, and on hardware an
; access in that window clears the lock flag, so the store conditional fails
; every time round and the loop never terminates.  The loops are therefore
; built after register allocation, where nothing can still insert a spill or a
; reload; before that they were built during selection and -O0 hung on the
; first atomic it reached.
;
; Every check below is the same shape: from the load locked to the store
; conditional, no load and no store of any kind.  The RUN lines cover -O0,
; whose allocator spills everything, as well as -O2.

; CHECK-LABEL: rmw64:
; CHECK:       ldq_l
; CHECK-NOT:   {{^[ \t]*(ld|st)[a-z_]*[ \t]}}
; CHECK:       stq_c
define i64 @rmw64(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v seq_cst
  ret i64 %r
}

; A compare-and-swap splits its window across two blocks, so the check spans
; the branch between them as well.
; CHECK-LABEL: cas64:
; CHECK:       ldq_l
; CHECK-NOT:   {{^[ \t]*(ld|st)[a-z_]*[ \t]}}
; CHECK:       stq_c
define i64 @cas64(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n seq_cst seq_cst
  %v = extractvalue { i64, i1 } %r, 0
  ret i64 %v
}
