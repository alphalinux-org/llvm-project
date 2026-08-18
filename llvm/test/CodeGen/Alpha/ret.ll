; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: v:
; CHECK:       ret
define void @v() {
  ret void
}

; CHECK-LABEL: id:
; CHECK:       bis $31, $16, $0
; CHECK-NEXT:  ret
define i64 @id(i64 %x) {
  ret i64 %x
}

; CHECK-LABEL: second:
; CHECK:       bis $31, $17, $0
; CHECK-NEXT:  ret
define i64 @second(i64 %x, i64 %y) {
  ret i64 %y
}

; CHECK-LABEL: fid:
; CHECK:       cpys $f16, $f16, $f0
; CHECK-NEXT:  ret
define double @fid(double %x) {
  ret double %x
}

; CHECK-LABEL: sfid:
; CHECK:       cpys $f16, $f16, $f0
; CHECK-NEXT:  ret
define float @sfid(float %x) {
  ret float %x
}

; A 128-bit integer is returned in memory, as it is under GCC: the caller passes
; the buffer in $16, the callee fills it in and hands the pointer back in $0.
; CHECK-LABEL: wide:
; CHECK-DAG:   bis $31, $16, $0
; CHECK-DAG:   stq $17, 0($0)
; CHECK-DAG:   stq $18, 8($0)
; CHECK:       ret
define i128 @wide(i128 %x) {
  ret i128 %x
}
