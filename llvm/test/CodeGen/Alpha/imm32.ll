; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Constants that do not fit in the 16-bit `lda` displacement but decompose into
; two 16-bit signed halves are materialized with an ldah/lda pair
; (value = (Hi << 16) + Lo).

; 100000 = 2 * 65536 - 31072
; CHECK-LABEL: pos:
; CHECK:       ldah $0, 2($31)
; CHECK-NEXT:  lda $0, -31072($0)
; CHECK-NEXT:  ret
define i64 @pos() {
  ret i64 100000
}

; CHECK-LABEL: neg:
; CHECK:       ldah $0, -2($31)
; CHECK-NEXT:  lda $0, 31072($0)
; CHECK-NEXT:  ret
define i64 @neg() {
  ret i64 -100000
}

; 65536 has a zero low half.
; CHECK-LABEL: shifted:
; CHECK:       ldah $0, 1($31)
; CHECK-NEXT:  lda $0, 0($0)
; CHECK-NEXT:  ret
define i64 @shifted() {
  ret i64 65536
}
