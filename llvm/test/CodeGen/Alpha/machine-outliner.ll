; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; A repeated instruction sequence in minsize functions is extracted into a
; shared function, called with a PC-relative bsr that saves the return address
; in $23; the outlined function returns with a jump through $23.

; CHECK-LABEL: f1:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; CHECK-LABEL: f2:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; CHECK-LABEL: f3:
; CHECK: bsr $23, OUTLINED_FUNCTION_0

define i64 @f1(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @f2(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @f3(i64 %a, i64 %b) minsize {
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
