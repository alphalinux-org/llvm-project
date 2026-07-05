; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A byte/word/longword extract, insert, or mask at a variable byte position is a
; single ext/ins/msk; the position register holds the byte index directly.
define i64 @extb(i64 %x, i64 %n) {
; CHECK-LABEL: extb:
; CHECK: extbl $16, $17, $0
; CHECK-NOT: srl
  %s = shl i64 %n, 3
  %sr = lshr i64 %x, %s
  %r = and i64 %sr, 255
  ret i64 %r
}
define i64 @insw(i64 %y, i64 %n) {
; CHECK-LABEL: insw:
; CHECK: inswl $16, $17, $0
  %b = and i64 %y, 65535
  %s = shl i64 %n, 3
  %r = shl i64 %b, %s
  ret i64 %r
}
define i64 @mskb(i64 %x, i64 %n) {
; CHECK-LABEL: mskb:
; CHECK: mskbl $16, $17, $0
; CHECK-NOT: bic
  %s = shl i64 %n, 3
  %m = shl i64 255, %s
  %nm = xor i64 %m, -1
  %r = and i64 %x, %nm
  ret i64 %r
}
