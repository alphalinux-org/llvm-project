; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; umulh gives the high half of an unsigned 64x64 multiply.
; CHECK-LABEL: mulhu:
; CHECK:       umulh $16, $17, $0
; CHECK:       ret
define i64 @mulhu(i64 %a, i64 %b) {
  %x = zext i64 %a to i128
  %y = zext i64 %b to i128
  %m = mul i128 %x, %y
  %s = lshr i128 %m, 64
  %r = trunc i128 %s to i64
  ret i64 %r
}

; The signed high multiply is expanded in terms of umulh.
; CHECK-LABEL: mulhs:
; CHECK:       umulh
; CHECK:       ret
define i64 @mulhs(i64 %a, i64 %b) {
  %x = sext i64 %a to i128
  %y = sext i64 %b to i128
  %m = mul i128 %x, %y
  %s = lshr i128 %m, 64
  %r = trunc i128 %s to i64
  ret i64 %r
}

; Unsigned division by a constant becomes a umulh-based multiply, no divq call.
; CHECK-LABEL: divconst:
; CHECK-NOT:   __divqu
; CHECK:       umulh
; CHECK:       ret
define i64 @divconst(i64 %a) {
  %r = udiv i64 %a, 7
  ret i64 %r
}
