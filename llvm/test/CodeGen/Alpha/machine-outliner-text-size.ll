; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s --check-prefix=OUTLINE
; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 \
; RUN:   -alpha-outliner-max-text-size=32 < %s | FileCheck %s --check-prefix=BIG

; The outlined call is a bsr, whose displacement reaches +/-4 MiB, and nothing
; relaxes it: BranchRelaxation runs in addPreEmitPass and the outliner runs
; after that.  A module whose text is near that size is therefore left alone
; rather than given a call that may not reach; machine-outliner-range.ll is the
; other half of this, showing that a call which does not reach is an assembler
; error and not a wrong answer.

; This module is far below the real limit, so it outlines as usual.
; OUTLINE: bsr $23, OUTLINED_FUNCTION_0

; With the limit lowered below this module's size, nothing is outlined and the
; sequence is left in place in each function.
; BIG-NOT: bsr $23, OUTLINED_FUNCTION
; BIG-NOT: OUTLINED_FUNCTION_0:

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
