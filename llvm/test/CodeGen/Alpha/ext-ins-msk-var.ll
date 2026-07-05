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

; The remaining six of the nine patterns.  The msk forms are the intricate ones
; -- and x, (not (shl mask, (shl n, 3))) -- so all three widths are covered.
define i64 @extw(i64 %x, i64 %n) {
; CHECK-LABEL: extw:
; CHECK: extwl $16, $17, $0
  %s = shl i64 %n, 3
  %sr = lshr i64 %x, %s
  %r = and i64 %sr, 65535
  ret i64 %r
}
define i64 @extl(i64 %x, i64 %n) {
; CHECK-LABEL: extl:
; CHECK: extll $16, $17, $0
  %s = shl i64 %n, 3
  %sr = lshr i64 %x, %s
  %r = and i64 %sr, 4294967295
  ret i64 %r
}
define i64 @insb(i64 %y, i64 %n) {
; CHECK-LABEL: insb:
; CHECK: insbl $16, $17, $0
  %b = and i64 %y, 255
  %s = shl i64 %n, 3
  %r = shl i64 %b, %s
  ret i64 %r
}
define i64 @insl(i64 %y, i64 %n) {
; CHECK-LABEL: insl:
; CHECK: insll $16, $17, $0
  %b = and i64 %y, 4294967295
  %s = shl i64 %n, 3
  %r = shl i64 %b, %s
  ret i64 %r
}
define i64 @mskw(i64 %x, i64 %n) {
; CHECK-LABEL: mskw:
; CHECK: mskwl $16, $17, $0
; CHECK-NOT: bic
  %s = shl i64 %n, 3
  %m = shl i64 65535, %s
  %nm = xor i64 %m, -1
  %r = and i64 %x, %nm
  ret i64 %r
}
define i64 @mskl(i64 %x, i64 %n) {
; CHECK-LABEL: mskl:
; CHECK: mskll $16, $17, $0
; CHECK-NOT: bic
  %s = shl i64 %n, 3
  %m = shl i64 4294967295, %s
  %nm = xor i64 %m, -1
  %r = and i64 %x, %nm
  ret i64 %r
}
