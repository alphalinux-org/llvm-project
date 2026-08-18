; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: sext8:
; CHECK:       sll $16, 56, $0
; CHECK-NEXT:  sra $0, 56, $0
; CHECK-NEXT:  ret
define i64 @sext8(i8 %x) {
  %r = sext i8 %x to i64
  ret i64 %r
}

; CHECK-LABEL: sext16:
; CHECK:       sll $16, 48, $0
; CHECK-NEXT:  sra $0, 48, $0
; CHECK-NEXT:  ret
define i64 @sext16(i16 %x) {
  %r = sext i16 %x to i64
  ret i64 %r
}
