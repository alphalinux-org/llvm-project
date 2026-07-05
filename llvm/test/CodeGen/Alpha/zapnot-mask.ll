; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; A byte-granular AND mask (each byte 0x00 or 0xFF) is a single zapnot.
define i64 @alt(i64 %x) {
; CHECK-LABEL: alt:
; CHECK: zapnot $16, 85, $0
; CHECK-NOT: and
  %r = and i64 %x, 71777214294589695
  ret i64 %r
}
define i64 @high(i64 %x) {
; CHECK-LABEL: high:
; CHECK: zapnot $16, 254, $0
; CHECK-NOT: and
  %r = and i64 %x, -256
  ret i64 %r
}

; 0xff is byte-granular too, but it is also an 8-bit literal, and and issues on
; every integer pipe where zapnot is restricted to the shift-capable ones.
define i64 @low_byte(i64 %x) {
; CHECK-LABEL: low_byte:
; CHECK: and $16, 255, $0
; CHECK-NOT: zapnot
  %r = and i64 %x, 255
  ret i64 %r
}
