; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; Signed division by a constant uses a magic-number multiply (umulh with a sign
; correction) instead of the __divq millicode call.  MULHS is custom-expanded
; from umulh so BuildSDIV can select it.

; CHECK-LABEL: sdiv10:
; CHECK-NOT:  __divq
; CHECK:      umulh
; CHECK:      ret
define i64 @sdiv10(i64 %x) {
  %r = sdiv i64 %x, 10
  ret i64 %r
}

; CHECK-LABEL: srem10:
; CHECK-NOT:  __remq
; CHECK:      umulh
; CHECK:      ret
define i64 @srem10(i64 %x) {
  %r = srem i64 %x, 10
  ret i64 %r
}

; A variable divisor still uses the millicode routine.
; CHECK-LABEL: sdivvar:
; CHECK:      __divq
define i64 @sdivvar(i64 %x, i64 %y) {
  %r = sdiv i64 %x, %y
  ret i64 %r
}

; The signed high multiply itself: umulh corrected by both sign terms.
; CHECK-LABEL: mulhs:
; CHECK-DAG:  umulh $16, $17,
; CHECK-DAG:  sra $16, 63,
; CHECK-DAG:  sra $17, 63,
; CHECK:      ret
define i64 @mulhs(i64 %a, i64 %b) {
  %m = sext i64 %a to i128
  %n = sext i64 %b to i128
  %p = mul i128 %m, %n
  %s = lshr i128 %p, 64
  %t = trunc i128 %s to i64
  ret i64 %t
}
