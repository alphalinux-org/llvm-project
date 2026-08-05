; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -stop-after=finalize-isel < %s \
; RUN:   | FileCheck %s --check-prefix=MIR

; The node has to be built as a memory node: selection reads the memory operand
; back off it to give to the instruction, so building it as a plain node left
; the instruction carrying whatever followed the node in memory.
; MIR: RMW_USTORE {{.*}} :: (store (s64) into %ir.p, align 1)
; MIR: RMW_USTORE {{.*}} :: (store (s64) into %ir.q, align 1)

; A misaligned store reads the quadwords its field falls in, splices the field
; in and writes them back.  Two such stores can fall in one quadword, so the
; reads of one must not be hoisted above the write-backs of the other: the
; sequence stays a single instruction until after scheduling.

; CHECK-LABEL: two_stores:
; CHECK:      ldq_u [[A:\$[0-9]+]], 0([[P:\$[0-9]+]])
; CHECK:      stq_u {{\$[0-9]+}}, 0({{\$[0-9]+}})
; CHECK:      stq_u [[A]], 0([[P]])
; CHECK:      ldq_u
; CHECK:      stq_u
; CHECK:      stq_u
define void @two_stores(ptr %p, ptr %q, i64 %a, i64 %b) {
  store i64 %a, ptr %p, align 1
  store i64 %b, ptr %q, align 1
  ret void
}
