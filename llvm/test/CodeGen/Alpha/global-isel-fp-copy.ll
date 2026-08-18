; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A float is loaded straight into the floating-point bank with lds, which also
; converts the S_floating format on the way in.  It used to go the long way --
; ldl into a general register, then a store and a reload to get it across the
; banks -- because the legalizer had no rule for a 32-bit load and widened it
; to a quadword, which only a general register can hold.
;
; The SelectionDAG path emits the same single lds.

declare i32 @bar(float)

; CHECK-LABEL: foo:
; CHECK:      lds $f16, 0($16)
; CHECK-NOT:  stl
; CHECK:      jsr $26, ($27)
define void @foo(ptr %p, ptr %q) {
  %f = load float, ptr %p
  %r = call i32 @bar(float %f)
  store i32 %r, ptr %q
  ret void
}
