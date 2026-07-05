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


; A linkonce_odr function may be deduplicated by the linker, so nothing is
; outlined from one unless the outliner is asked for it -- even though the
; sequence is the one already extracted above.
; CHECK-LABEL: lo_a:
; CHECK-NOT: bsr
; CHECK-LABEL: lo_b:
; CHECK-NOT: bsr
; CHECK-LABEL: lo_c:
; CHECK-NOT: bsr
define linkonce_odr i64 @lo_a(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define linkonce_odr i64 @lo_b(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define linkonce_odr i64 @lo_c(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}

; A function pinned to a section may expect all of its code to stay there, so
; it is left alone too.
; CHECK-LABEL: sec_a:
; CHECK-NOT: bsr
; CHECK-LABEL: sec_b:
; CHECK-NOT: bsr
; CHECK-LABEL: sec_c:
; CHECK-NOT: bsr
define i64 @sec_a(i64 %a, i64 %b) minsize section ".mine" {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @sec_b(i64 %a, i64 %b) minsize section ".mine" {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
define i64 @sec_c(i64 %a, i64 %b) minsize section ".mine" {
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
