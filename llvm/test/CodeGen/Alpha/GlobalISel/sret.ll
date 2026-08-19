; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A function returning in memory hands the buffer pointer it was given back in
; $0.  The calling convention tables say nothing about this, so both paths do it
; by hand and this test is what keeps them agreeing.

%struct.big = type { i64, i64, i64 }

; CHECK-LABEL: fill:
; CHECK:       bis $31, $16, $0
define void @fill(ptr sret(%struct.big) %ret, i64 %x) {
  store i64 %x, ptr %ret
  ret void
}
