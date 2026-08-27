; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s

; An extension whose source is not a power of two.  Nothing produces one from
; the front end -- it comes out of the legalizer, which lowers a load of an odd
; size into loads of the pieces and an extension from the odd width -- and
; there is no way to legalize it away afterwards: LegalizerHelper has no
; widenScalar case for an extension at all, and lowerEXT handles only vectors.
; So the width is legal and the selector extends it, or the whole function
; falls back to the SelectionDAG path.  -global-isel-abort=1 is what asks.

define i64 @sext_i24(ptr %p) {
; CHECK-LABEL: sext_i24:
; CHECK:       sll $0, 40, $0
; CHECK-NEXT:  sra $0, 40, $0
  %v = load i24, ptr %p, align 4
  %s = sext i24 %v to i64
  ret i64 %s
}

define i64 @zext_i24(ptr %p) {
; CHECK-LABEL: zext_i24:
; CHECK-NOT:   sra
  %v = load i24, ptr %p, align 4
  %z = zext i24 %v to i64
  ret i64 %z
}
