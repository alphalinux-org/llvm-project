; A load to R31/F31 is a software-directed prefetch only on the 21264 and later;
; on earlier processors it is an ordinary faulting load, so the prefetch is
; dropped.  The evict-next hint (ldq/ldt) is defined only on the 21364.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s --check-prefix=EV6
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -mattr=+prefetch-evict-next \
; RUN:   < %s | FileCheck %s --check-prefix=EV7

declare void @llvm.prefetch(ptr, i32, i32, i32)

define void @prefetches(ptr %p) {
; EV4-LABEL: prefetches:
; EV4-NOT: $31
;
; EV6-LABEL: prefetches:
; EV6-DAG: ldl $31, 0($16)
; EV6-DAG: ldl $31, 64($16)
; Both write prefetches become lds here: without the evict-next hint the
; locality-0 one has no encoding of its own, so it is not folded away and the
; count has to be checked rather than a single occurrence.
; EV6-COUNT-2: lds $f31, 0($16)
; EV6-NOT: ldq $31
; EV6-NOT: ldt $f31
;
; EV7-LABEL: prefetches:
; EV7-DAG: ldl $31, 0($16)
; EV7-DAG: ldq $31, 64($16)
; EV7-DAG: lds $f31, 0($16)
; EV7-DAG: ldt $f31, 0($16)
  call void @llvm.prefetch(ptr %p, i32 0, i32 3, i32 1)
  %q = getelementptr i8, ptr %p, i64 64
  call void @llvm.prefetch(ptr %q, i32 0, i32 0, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 3, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 0, i32 1)
  ret void
}
