; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; zapnot provides byte-granular zero-extension.  The 8-bit mask keeps the low
; N bytes, so masking with 0xff / 0xffff / 0xffffffff (and zero-extending a
; narrow value) becomes a single zapnot instead of materializing a wide mask.

; CHECK-LABEL: mask8:
; CHECK:       zapnot $16, 1, $0
; CHECK-NEXT:  ret
define i64 @mask8(i64 %x) {
  %r = and i64 %x, 255
  ret i64 %r
}

; CHECK-LABEL: mask16:
; CHECK:       zapnot $16, 3, $0
; CHECK-NEXT:  ret
define i64 @mask16(i64 %x) {
  %r = and i64 %x, 65535
  ret i64 %r
}

; CHECK-LABEL: mask32:
; CHECK:       zapnot $16, 15, $0
; CHECK-NEXT:  ret
define i64 @mask32(i64 %x) {
  %r = and i64 %x, 4294967295
  ret i64 %r
}

; CHECK-LABEL: zext32:
; CHECK:       zapnot $16, 15, $0
; CHECK-NEXT:  ret
define i64 @zext32(i32 %x) {
  %r = zext i32 %x to i64
  ret i64 %r
}
