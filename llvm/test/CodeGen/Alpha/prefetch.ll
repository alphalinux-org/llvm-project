; A load to R31/F31 is a software-directed prefetch only on the 21264 and later;
; on earlier processors it is an ordinary faulting load, so the prefetch is
; dropped.  The 21264 defines all three hints at once, so -mcpu=ev6 gets them.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s --check-prefix=EV6

declare void @llvm.prefetch(ptr, i32, i32, i32)

define void @prefetches(ptr %p) {
; EV4-LABEL: prefetches:
; EV4-NOT: $31
;
; EV6-LABEL: prefetches:
; EV6-DAG: ldl $31, 0($16)
; A read with no temporal locality asks for the line to be evicted next.
; EV6-DAG: ldq $31, 64($16)
; Both write prefetches keep modify intent: there is no encoding that asks for
; modify intent and evict next together, and giving up the intent would fetch
; the line read-shared and leave the store to upgrade it.  Neither is folded
; away, so the count is what has to be checked.
; EV6-COUNT-2: lds $f31, 0($16)
; ldt to F31 is in the manual's normal-prefetch list, not the evict-next one,
; so it says nothing a plain ldl does not and is never emitted.
; EV6-NOT: ldt $f31
  call void @llvm.prefetch(ptr %p, i32 0, i32 3, i32 1)
  %q = getelementptr i8, ptr %p, i64 64
  call void @llvm.prefetch(ptr %q, i32 0, i32 0, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 3, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 0, i32 1)
  ret void
}
