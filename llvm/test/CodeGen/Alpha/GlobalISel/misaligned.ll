; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=0 < %s | FileCheck %s

; A two-byte store at an odd address can straddle a quadword boundary, so it
; takes a read-modify-write of both quadwords.  GlobalISel does not build that,
; and must not settle for an access that would update only one of them.

; CHECK-LABEL: store_misaligned_i16:
; CHECK:      ldq_u
; CHECK:      ldq_u
; CHECK:      stq_u
; CHECK:      stq_u
define void @store_misaligned_i16(ptr %p, i16 %v) {
  store i16 %v, ptr %p, align 1
  ret void
}

; The load side of the same thing.  It reads both quadwords the datum can fall
; in and splices the halves together; an ldq alone would fault or read the
; wrong bytes.  This was reaching the selector and failing there, because every
; alignment in the legalizer's load rule was written as one byte -- see
; legalize-unsupported.mir.

; CHECK-LABEL: load_misaligned_i64:
; CHECK:      ldq_u
; CHECK:      extql
; CHECK:      ldq_u
; CHECK:      extqh
; CHECK:      bis
define i64 @load_misaligned_i64(ptr %p) {
  %v = load i64, ptr %p, align 1
  ret i64 %v
}
