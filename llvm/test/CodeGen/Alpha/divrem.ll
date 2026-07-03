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
; CHECK:       bis $31, $16, $24
; CHECK:       bis $31, $17, $25
; CHECK:       jsr $23, ($27)
; CHECK:       bis $31, $27, $0
; CHECK:       ret
define i64 @udiv(i64 %a, i64 %b) {
  %r = udiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: srem:
; CHECK:       ldq $27, __remq($29){{.*}}!literal
; CHECK:       bis $31, $16, $24
; CHECK:       bis $31, $17, $25
; CHECK:       jsr $23, ($27)
; CHECK:       bis $31, $27, $0
; CHECK:       ret
define i64 @srem(i64 %a, i64 %b) {
  %r = srem i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem:
; CHECK:       ldq $27, __remqu($29){{.*}}!literal
; CHECK:       bis $31, $16, $24
; CHECK:       bis $31, $17, $25
; CHECK:       jsr $23, ($27)
; CHECK:       bis $31, $27, $0
; CHECK:       ret
define i64 @urem(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}

; A 32-bit division is the quadword routine applied to extended operands: the
; millicode reads all 64 bits of $24 and $25, so a value that is not already
; extended has to be, and which extension it is depends on the signedness.  The
; signed one is addl with $31, and the quotient it returns comes back extended
; the same way.
; CHECK-LABEL: sdiv32:
; CHECK-DAG:   addl $16, $31, $24
; CHECK-DAG:   addl $17, $31, $25
; CHECK:       ldq $27, __divq($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
; CHECK:       ret
define signext i32 @sdiv32(i32 %a, i32 %b) {
  %r = sdiv i32 %a, %b
  ret i32 %r
}

; The unsigned one is the zero extension, and the unsigned routine.
; CHECK-LABEL: udiv32:
; CHECK-DAG:   zapnot $16, 15, $24
; CHECK-DAG:   zapnot $17, 15, $25
; CHECK:       ldq $27, __divqu($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
; CHECK:       ret
define zeroext i32 @udiv32(i32 %a, i32 %b) {
  %r = udiv i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: srem32:
; CHECK-DAG:   addl $16, $31, $24
; CHECK-DAG:   addl $17, $31, $25
; CHECK:       ldq $27, __remq($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
define signext i32 @srem32(i32 %a, i32 %b) {
  %r = srem i32 %a, %b
  ret i32 %r
}

; CHECK-LABEL: urem32:
; CHECK-DAG:   zapnot $16, 15, $24
; CHECK-DAG:   zapnot $17, 15, $25
; CHECK:       ldq $27, __remqu($29){{.*}}!literal
; CHECK:       jsr $23, ($27)
define zeroext i32 @urem32(i32 %a, i32 %b) {
  %r = urem i32 %a, %b
  ret i32 %r
}
