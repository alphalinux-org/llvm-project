; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: big:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldah $0, .LCPI0_0($29){{.*}}!gprelhigh
; CHECK:       ldq $0, .LCPI0_0($0){{.*}}!gprellow
; CHECK:       ret
define i64 @big() {
  ret i64 1234605616436508552
}
