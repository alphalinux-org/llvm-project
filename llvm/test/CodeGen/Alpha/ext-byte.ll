; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Extracting a byte/word/longword at a constant byte offset is one ext.
define i64 @byte1(i64 %x) {
; CHECK-LABEL: byte1:
; CHECK: extbl $16, 1, $0
; CHECK-NOT: srl
  %s = lshr i64 %x, 8
  %r = and i64 %s, 255
  ret i64 %r
}
define i64 @word2(i64 %x) {
; CHECK-LABEL: word2:
; CHECK: extwl $16, 2, $0
  %s = lshr i64 %x, 16
  %r = and i64 %s, 65535
  ret i64 %r
}
define i64 @long1(i64 %x) {
; CHECK-LABEL: long1:
; CHECK: extll $16, 1, $0
  %s = lshr i64 %x, 8
  %r = and i64 %s, 4294967295
  ret i64 %r
}
