; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s \
; RUN:   --check-prefix=NOFP --implicit-check-not='{{bis \$31, \$30, \$15}}'
; RUN: llc -mtriple=alpha-unknown-linux-gnu -frame-pointer=all < %s \
; RUN:   | FileCheck %s

; -fno-omit-frame-pointer reaches the back end as DisableFramePointerElim, and
; a function with neither a variable-sized object nor a taken frame address is
; the only case where it is observable: without it the option is silently
; ignored.

declare void @opaque()

define void @leaf_with_frame_pointer() {
; CHECK-LABEL: leaf_with_frame_pointer:
; The frame pointer is established after $15 itself is saved.
; CHECK:       stq $15,
; CHECK:       bis $31, $30, $15
; NOFP-LABEL: leaf_with_frame_pointer:
  call void @opaque()
  ret void
}
