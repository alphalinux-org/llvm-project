; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: five:
; CHECK:       lda $0, 5($31)
; CHECK-NEXT:  ret
define i64 @five() {
  ret i64 5
}

; CHECK-LABEL: neg:
; CHECK:       lda $0, -7($31)
; CHECK-NEXT:  ret
define i64 @neg() {
  ret i64 -7
}

; CHECK-LABEL: addimm:
; CHECK:       lda $0, 10($31)
; CHECK:       addq $16, $0, $0
; CHECK-NEXT:  ret
define i64 @addimm(i64 %x) {
  %r = add i64 %x, 10
  ret i64 %r
}
