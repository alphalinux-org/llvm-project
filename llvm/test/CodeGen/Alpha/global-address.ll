; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Global addresses use the large-data (GOT) model: the function establishes the
; global pointer with ldgp, then loads the address from the GOT slot with an
; R_ALPHA_LITERAL relocation (spelled !literal for gas).

@g = global i64 0

; CHECK-LABEL: addr:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldq $0, g($29){{.*}}!literal
; CHECK:       ret
define ptr @addr() {
  ret ptr @g
}

; CHECK-LABEL: load:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldq $0, g($29){{.*}}!literal
; CHECK:       ldq $0, 0($0)
; CHECK:       ret
define i64 @load() {
  %v = load i64, ptr @g
  ret i64 %v
}

; A leaf function that touches no global needs no ldgp.
; CHECK-LABEL: nogp:
; CHECK-NOT:   ldgp
; CHECK:       addq $16, $17, $0
; CHECK:       ret
define i64 @nogp(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}
