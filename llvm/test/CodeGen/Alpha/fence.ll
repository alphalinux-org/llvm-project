; RUN: llc -mtriple=alpha-unknown-linux-gnu -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

; A cross-thread fence is an mb, whatever its ordering: Alpha's memory model
; reorders everything a barrier does not pin down, so acquire and release get
; the same instruction as sequential consistency.

; CHECK-LABEL: seq_cst:
; CHECK: mb
define void @seq_cst() {
  fence seq_cst
  ret void
}

; CHECK-LABEL: acquire:
; CHECK: mb
define void @acquire() {
  fence acquire
  ret void
}

; CHECK-LABEL: release:
; CHECK: mb
define void @release() {
  fence release
  ret void
}

; A single-thread fence orders nothing another processor can observe.  It has
; to stop the compiler from moving accesses across it, which costs no
; instruction; emitting an mb for one is a real barrier bought for nothing.
; CHECK-LABEL: singlethread:
; CHECK-NOT: mb
; CHECK: ret
define void @singlethread() {
  fence syncscope("singlethread") seq_cst
  ret void
}

; The compiler barrier is still there: the two volatile stores may not be
; reordered or merged across it.
; CHECK-LABEL: singlethread_ordering:
; CHECK:      stq $17, 0($16)
; CHECK-NOT:  mb
; CHECK:      stq $18, 0($16)
define void @singlethread_ordering(ptr %p, i64 %a, i64 %b) {
  store volatile i64 %a, ptr %p
  fence syncscope("singlethread") seq_cst
  store volatile i64 %b, ptr %p
  ret void
}
