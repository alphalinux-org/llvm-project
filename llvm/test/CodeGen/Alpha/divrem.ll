; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Alpha has no integer division instruction; division and remainder call the
; millicode routines with the operands in $24/$25, entered through $23, result
; in $27.

; CHECK-LABEL: sdiv:
; CHECK:       ldq $27, __divq($29){{.*}}!literal
; CHECK:       bis $31, $16, $24
; CHECK:       bis $31, $17, $25
; CHECK:       jsr $23, ($27)
; CHECK:       bis $31, $27, $0
; CHECK:       ret
define i64 @sdiv(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: udiv:
; CHECK:       ldq $27, __divqu($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
; CHECK:       ret
define i64 @udiv(i64 %a, i64 %b) {
  %r = udiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: srem:
; CHECK:       ldq $27, __remq($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
; CHECK:       ret
define i64 @srem(i64 %a, i64 %b) {
  %r = srem i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem:
; CHECK:       ldq $27, __remqu($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
; CHECK:       ret
define i64 @urem(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}
