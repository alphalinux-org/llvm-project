; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; A repeated instruction sequence in minsize functions is extracted into a
; shared function, called with a PC-relative bsr that saves the return address
; in $23; the outlined function returns with a jump through $23.

; CHECK-LABEL: outline_a:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; CHECK-LABEL: outline_b:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; CHECK-LABEL: outline_c:
; CHECK: bsr $23, OUTLINED_FUNCTION_0

define i64 @outline_a(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @outline_b(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @outline_c(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}

; A function that is not built for minimum size is left alone.
; CHECK-LABEL: big:
; CHECK-NOT: bsr
; CHECK: ret
define i64 @big(i64 %a, i64 %b) {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}

; The outlined function ends by jumping back through $23.
; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK: jmp $31, ($23), 0
