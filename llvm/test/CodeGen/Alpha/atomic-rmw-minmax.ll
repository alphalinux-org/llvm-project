; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The read-modify-writes with no ldq_l/stq_c inserter of their own -- the
; min/max forms and nand -- are open-coded by the atomic expander as a
; compare-and-swap retry loop: read with ldq_l, compare, select, and store back
; with stq_c, retrying both when the value changed under us and when stq_c
; reports a lost reservation.

; CHECK-LABEL: min:
; CHECK:       cmple
; CHECK:       cmov
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
; CHECK:       ret
define i64 @min(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: max:
; CHECK:       cmplt
; CHECK:       cmov
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
define i64 @max(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v monotonic
  ret i64 %r
}

; The unsigned forms compare with cmpule/cmpult rather than cmple/cmplt.
; CHECK-LABEL: umin:
; CHECK:       cmpule
; CHECK:       cmov
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
define i64 @umin(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: umax:
; CHECK:       cmpult
; CHECK:       cmov
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
define i64 @umax(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v monotonic
  ret i64 %r
}

; nand is an and followed by a complement; ornot against the zero register is
; the Alpha spelling of the latter.
; CHECK-LABEL: nand:
; CHECK:       and
; CHECK:       ornot $31,
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
define i64 @nand(ptr %p, i64 %v) {
  %r = atomicrmw nand ptr %p, i64 %v monotonic
  ret i64 %r
}

; A sub-word unsigned compare is done on sign-extended operands, which is
; correct because sign extension preserves the unsigned ordering of a byte:
; it maps [0,127] to itself and [128,255] above it, the same order those two
; halves have as unsigned bytes.  Without BWX the extension is a pair of
; shifts rather than sextb.
; CHECK-LABEL: umin_i8:
; CHECK:       sll {{\$[0-9]+}}, 56,
; CHECK:       cmpule
; CHECK:       ldq_l
; CHECK:       stq_c
define i8 @umin_i8(ptr %p, i8 %v) {
  %r = atomicrmw umin ptr %p, i8 %v monotonic
  ret i8 %r
}

; CHECK-LABEL: umax_i16:
; CHECK:       sll {{\$[0-9]+}}, 48,
; CHECK:       cmpult
; CHECK:       ldq_l
; CHECK:       stq_c
define i16 @umax_i16(ptr %p, i16 %v) {
  %r = atomicrmw umax ptr %p, i16 %v monotonic
  ret i16 %r
}

; CHECK-LABEL: min_i32:
; CHECK:       ldl_l
; CHECK:       stl_c
define i32 @min_i32(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v monotonic
  ret i32 %r
}
