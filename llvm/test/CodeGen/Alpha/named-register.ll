; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A global register variable bound to a numbered register (the kernel uses
; "register T *p __asm__("$8")" for the current-thread pointer) reads that
; register directly.

; CHECK-LABEL: cur:
; CHECK: bis $31, $8, $0
define i64 @cur() {
  %v = call i64 @llvm.read_register.i64(metadata !0)
  ret i64 %v
}

declare i64 @llvm.read_register.i64(metadata)

!llvm.named.register.$8 = !{!0}
!0 = !{!"$8"}
