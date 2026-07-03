; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
;
; Alpha shift instructions use only the low 6 bits of the shift amount, so an
; AND mask whose low 6 bits are all 1 is redundant.  Removing it is the generic
; demanded-bits fold rather than anything added here; what the patterns below
; decide is that it applies, by selecting shl/srl/sra to instructions that
; behave that way.  A mask that clears any of those 6 bits is semantically
; meaningful and must survive.

; y & 0x3f (= y % 64): all 6 low bits set -- AND is redundant.
define i64 @srl_mod64(i64 %x, i64 %y) {
; CHECK-LABEL: srl_mod64:
; CHECK-NOT: and
; CHECK: srl
  %m = and i64 %y, 63
  %r = lshr i64 %x, %m
  ret i64 %r
}

; y & 0x7f (= y % 128): low 6 bits all set -- AND is still redundant.
define i64 @srl_mod128(i64 %x, i64 %y) {
; CHECK-LABEL: srl_mod128:
; CHECK-NOT: and
; CHECK: srl
  %m = and i64 %y, 127
  %r = lshr i64 %x, %m
  ret i64 %r
}

; y & 0x1f (= y % 32): bit 5 cleared -- AND is semantically meaningful.
define i64 @srl_mod32(i64 %x, i64 %y) {
; CHECK-LABEL: srl_mod32:
; CHECK: and
; CHECK: srl
  %m = and i64 %y, 31
  %r = lshr i64 %x, %m
  ret i64 %r
}
