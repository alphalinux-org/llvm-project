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
