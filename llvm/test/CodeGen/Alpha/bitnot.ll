; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: nott:
; CHECK:       ornot $31, $16, $0
; CHECK-NEXT:  ret
define i64 @nott(i64 %x) {
  %r = xor i64 %x, -1
  ret i64 %r
}

; CHECK-LABEL: andnot:
; CHECK:       bic $16, $17, $0
; CHECK-NEXT:  ret
define i64 @andnot(i64 %a, i64 %b) {
  %n = xor i64 %b, -1
  %r = and i64 %a, %n
  ret i64 %r
}

; CHECK-LABEL: ornot:
; CHECK:       ornot $16, $17, $0
; CHECK-NEXT:  ret
define i64 @ornot(i64 %a, i64 %b) {
  %n = xor i64 %b, -1
  %r = or i64 %a, %n
  ret i64 %r
}

; CHECK-LABEL: xnor:
; CHECK:       eqv $16, $17, $0
; CHECK-NEXT:  ret
define i64 @xnor(i64 %a, i64 %b) {
  %x = xor i64 %a, %b
  %r = xor i64 %x, -1
  ret i64 %r
}
