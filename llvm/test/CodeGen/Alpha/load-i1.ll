; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; A boolean occupies a byte in memory, so an i1 load is performed as a byte
; load; the result is used with only its low bit significant.

; CHECK-LABEL: load_bool:
; CHECK:       ldbu {{\$[0-9]+}}, 0($16)
define zeroext i1 @load_bool(ptr %p) {
  %v = load i1, ptr %p
  ret i1 %v
}

; CHECK-LABEL: use_bool:
; CHECK:       ldbu
define i64 @use_bool(ptr %p) {
  %b = load i1, ptr %p
  %r = select i1 %b, i64 10, i64 20
  ret i64 %r
}
