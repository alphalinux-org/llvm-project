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

; A complex float or complex double is the exception GCC makes to returning
; anything wider than a register in memory: it judges such a type by the width
; of one part, so the two halves come back in $f0/$f1.  Everything else that
; does not fit -- an aggregate, __int128, X_floating -- goes through the hidden
; pointer CanLowerReturn asks the generic code for, which is what keeps CCState
; from killing the compiler over a second return register that does not exist.
; CHECK-LABEL: two_doubles:
; CHECK-DAG:   cpys $f16, $f16, $f0
; CHECK-DAG:   cpys $f17, $f17, $f1
; CHECK:       ret
define { double, double } @two_doubles(double %a, double %b) {
  %r0 = insertvalue { double, double } poison, double %a, 0
  %r1 = insertvalue { double, double } %r0, double %b, 1
  ret { double, double } %r1
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
