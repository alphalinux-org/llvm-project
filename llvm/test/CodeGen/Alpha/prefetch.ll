; A load to R31/F31 is a software-directed prefetch only on the 21264 and later;
; on earlier processors it is an ordinary faulting load, so the prefetch is
; dropped.  There are four hints and -mcpu=ev6 gets all four: the fourth,
; ldt to F31, is implemented as modify-intent-plus-evict-next on the 21364
; alone, but a 21264 decodes it as a normal prefetch rather than faulting, so
; it is emitted unconditionally the way gcc emits it.
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
; A write prefetch takes modify intent, and takes the evict-next form of it when
; the locality operand says the line is wanted once.
; EV6-DAG: lds $f31, 0($16)
; EV6-DAG: ldt $f31, 0($16)
  call void @llvm.prefetch(ptr %p, i32 0, i32 3, i32 1)
  %q = getelementptr i8, ptr %p, i64 64
  call void @llvm.prefetch(ptr %q, i32 0, i32 0, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 3, i32 1)
  call void @llvm.prefetch(ptr %p, i32 1, i32 0, i32 1)
  ret void
}
