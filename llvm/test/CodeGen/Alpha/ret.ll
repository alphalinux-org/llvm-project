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
