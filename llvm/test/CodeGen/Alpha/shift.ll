; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: shl:
; CHECK:       sll $16, $17, $0
; CHECK-NEXT:  ret
define i64 @shl(i64 %x, i64 %y) {
  %r = shl i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: lshr:
; CHECK:       srl $16, $17, $0
; CHECK-NEXT:  ret
define i64 @lshr(i64 %x, i64 %y) {
  %r = lshr i64 %x, %y
  ret i64 %r
}

; CHECK-LABEL: ashr:
; CHECK:       sra $16, $17, $0
; CHECK-NEXT:  ret
define i64 @ashr(i64 %x, i64 %y) {
  %r = ashr i64 %x, %y
  ret i64 %r
}
