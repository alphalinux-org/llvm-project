; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Argument and return lowering for the OSF/ELF ABI: integer arguments arrive in
; $16-$21 and return in $0; floating-point arguments arrive in $f16-$f21 and
; return in $f0.

; CHECK-LABEL: v:
; CHECK:       ret $31, ($26), 1
define void @v() {
  ret void
}

; CHECK-LABEL: id:
; CHECK:       bis $31, $16, $0
; CHECK-NEXT:  ret $31, ($26), 1
define i64 @id(i64 %x) {
  ret i64 %x
}

; CHECK-LABEL: second:
; CHECK:       bis $31, $17, $0
; CHECK-NEXT:  ret $31, ($26), 1
define i64 @second(i64 %x, i64 %y) {
  ret i64 %y
}

; CHECK-LABEL: fid:
; CHECK:       cpys $f16, $f16, $f0
; CHECK-NEXT:  ret $31, ($26), 1
define double @fid(double %x) {
  ret double %x
}

; CHECK-LABEL: sfid:
; CHECK:       cpys $f16, $f16, $f0
; CHECK-NEXT:  ret $31, ($26), 1
define float @sfid(float %x) {
  ret float %x
}
