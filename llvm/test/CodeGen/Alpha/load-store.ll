; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: ldq:
; CHECK:       ldq $0, 0($16)
; CHECK-NEXT:  ret
define i64 @ldq(ptr %p) {
  %v = load i64, ptr %p
  ret i64 %v
}

; CHECK-LABEL: stq:
; CHECK:       stq $17, 0($16)
; CHECK-NEXT:  ret
define void @stq(ptr %p, i64 %v) {
  store i64 %v, ptr %p
  ret void
}

; CHECK-LABEL: ldq_off:
; CHECK:       ldq $0, 24($16)
; CHECK-NEXT:  ret
define i64 @ldq_off(ptr %p) {
  %q = getelementptr i64, ptr %p, i64 3
  %v = load i64, ptr %q
  ret i64 %v
}

; CHECK-LABEL: ldt:
; CHECK:       ldt $f0, 0($16)
; CHECK-NEXT:  ret
define double @ldt(ptr %p) {
  %v = load double, ptr %p
  ret double %v
}

; CHECK-LABEL: stt:
; CHECK:       stt $f17, 0($16)
; CHECK-NEXT:  ret
define void @stt(ptr %p, double %v) {
  store double %v, ptr %p
  ret void
}

; CHECK-LABEL: lds:
; CHECK:       lds $f0, 0($16)
; CHECK-NEXT:  ret
define float @lds(ptr %p) {
  %v = load float, ptr %p
  ret float %v
}
