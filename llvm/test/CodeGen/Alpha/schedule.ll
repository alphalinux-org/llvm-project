; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -O2 < %s | FileCheck %s

; The ordering is the whole point of a scheduling model, so these are ordered
; checks.  Order-independent ones would let the test pass with no model at all.
;
; The 21264 is out of order and its multiply is short, so it has no reason to
; hoist one ahead of the independent work that could hide it.

; CHECK-LABEL: imul_hide:
; CHECK:      %bb.0:
; CHECK-NEXT: addq $19, $20,
; CHECK-NEXT: mulq
; CHECK:      ret
define i64 @imul_hide(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e) {
  %m = mul i64 %a, %b
  %s = add i64 %m, %c
  %t = add i64 %d, %e
  %r = add i64 %s, %t
  ret i64 %r
}
