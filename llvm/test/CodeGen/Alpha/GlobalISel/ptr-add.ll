; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 -O2 < %s | FileCheck %s

; lda adds a sixteen-bit signed displacement to a register, so a pointer moved
; by a constant that fits is one instruction.  Nothing folds this before the
; selector -- no combine rewrites a G_PTR_ADD of a constant -- so the constant
; would otherwise be materialised into a register of its own and added, which
; is two.
;
; This is the same fold the load and store selection already does to reach a
; displacement; what is left for here is the offsets that never reach a memory
; operand.

; CHECK-LABEL: field:
; CHECK:       lda $0, 16($16)
; CHECK-NEXT:  ret
define ptr @field(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 16
  ret ptr %q
}

; CHECK-LABEL: neg:
; CHECK:       lda $0, -8($16)
; CHECK-NEXT:  ret
define ptr @neg(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 -8
  ret ptr %q
}

; Past what a displacement holds, the constant has to be built and added.
; CHECK-LABEL: far:
; CHECK:       ldah $[[T:[0-9]+]], 2($31)
; CHECK-NEXT:  lda $[[T]], -31072($[[T]])
; CHECK-NEXT:  addq $16, $[[T]], $0
define ptr @far(ptr %p) {
  %q = getelementptr i8, ptr %p, i64 100000
  ret ptr %q
}
