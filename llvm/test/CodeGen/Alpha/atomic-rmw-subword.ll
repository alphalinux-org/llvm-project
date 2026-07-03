; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Sub-word atomic read-modify-writes do their update inside an ldq_l/stq_c loop:
; extract the field, apply the operation, splice it back into the quadword.

; CHECK-LABEL: add8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK:       extbl {{\$[0-9]+}}, $16, $0
; CHECK:       addq $0, $17,
; CHECK:       mskbl {{\$[0-9]+}}, $16,
; CHECK:       insbl {{\$[0-9]+}}, $16,
; CHECK:       stq_c {{\$[0-9]+}}, 0([[A]])
; CHECK:       beq
; CHECK:       ret
define i8 @add8(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v monotonic
  ret i8 %r
}

; The xchg operation is loop-invariant, so the position/extract steps are
; hoisted out of the loop.
; CHECK-LABEL: xchg16:
; CHECK-DAG:   inswl {{\$[0-9]+}}, $16,
; CHECK:       ldq_l
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK:       stq_c
; CHECK:       beq
; CHECK:       extwl {{\$[0-9]+}}, $16, $0
; CHECK:       ret
define i16 @xchg16(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v monotonic
  ret i16 %r
}
