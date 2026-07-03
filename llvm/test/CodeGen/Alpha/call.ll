; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A call loads the callee address from the GOT into the procedure value ($27),
; jumps to it with jsr, and reloads the caller's global pointer afterwards.  A
; function that makes calls saves and restores its return address ($26).

declare i64 @f(i64, i64)

; CHECK-LABEL: call_add:
; CHECK:       ldgp $29, 0($27)
; CHECK:       stq $26, {{[0-9]+}}($30)
; CHECK:       ldq $27, f($29){{.*}}!literal
; CHECK:       jsr $26, ($27)
; CHECK:       ldgp $29, 0($26)
; CHECK:       addq $0, 1, $0
; CHECK:       ldq $26, {{[0-9]+}}($30)
; CHECK:       ret
define i64 @call_add(i64 %a, i64 %b) {
  %r = call i64 @f(i64 %a, i64 %b)
  %s = add i64 %r, 1
  ret i64 %s
}

declare void @sink(i64)

; CHECK-LABEL: call_void:
; CHECK:       jsr $26, ($27)
; CHECK:       ret
define void @call_void(i64 %x) {
  call void @sink(i64 %x)
  ret void
}

; A leaf function makes no call and needs no return-address save.
; CHECK-LABEL: leaf:
; CHECK-NOT:   jsr
; CHECK-NOT:   stq $26,
; CHECK:       ret
define i64 @leaf(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}
