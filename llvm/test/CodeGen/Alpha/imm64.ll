; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A constant too large for the ldah/lda pair is placed in the constant pool and
; loaded GP-relative with ldq.

; CHECK-LABEL: big:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldah $0, .LCPI0_0($29){{.*}}!gprelhigh
; CHECK:       lda $0, .LCPI0_0($0){{.*}}!gprellow
; CHECK:       ldq $0, 0($0)
; CHECK:       ret
define i64 @big() {
  ret i64 1234605616436508552
}
