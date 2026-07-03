; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; The return instruction does not list $26 as a use (the return address is only
; materialized in the encoding, and the epilogue reloads it as a frame-destroy
; instruction), so the machine verifier accepts a call-then-return function
; before prologue/epilogue insertion.

declare i64 @callee(i64)

; A leaf function returns via the incoming $26 with no frame.
; CHECK-LABEL: leaf:
; CHECK:      ret
define i64 @leaf(i64 %x) {
  %r = add i64 %x, 1
  ret i64 %r
}

; A non-leaf function saves $26 in the prologue and reloads it in the epilogue
; before returning.
; CHECK-LABEL: nonleaf:
; CHECK:      stq $26, 8($30)
; CHECK:      jsr $26, ($27)
; CHECK:      ldq $26, 8($30)
; CHECK:      ret
define i64 @nonleaf(i64 %x) {
  %r = call i64 @callee(i64 %x)
  %s = add i64 %r, 1
  ret i64 %s
}
