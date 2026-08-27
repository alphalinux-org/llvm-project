; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=0 < %s | FileCheck %s

; What GlobalISel deliberately does not handle, and hands to the SelectionDAG
; path whole.  Each of these needs machinery the selector has no counterpart
; for, so the legalizer marks it unsupported rather than calling it legal: an
; unsupported opcode is a clean fall back, where a legal one reaches a selector
; with nothing to select and stops the compilation.
;
; The output below is the SelectionDAG lowering, which is what makes the point:
; these compile correctly, just not through GlobalISel.

; There is no divide instruction; the SelectionDAG path calls __divq, which
; takes its arguments in $24/$25 and returns in $27.
; CHECK-LABEL: sdiv:
; CHECK:       __divq
define i64 @sdiv(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: urem:
; CHECK:       __remqu
define i64 @urem(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}

; umulh is an instruction and a signed high multiply is not: the overflow
; intrinsic lowers through G_SMULH, which the SelectionDAG path open-codes.
; CHECK-LABEL: smulo:
; CHECK:       umulh
define i1 @smulo(i64 %a, i64 %b) {
  %r = call {i64, i1} @llvm.smul.with.overflow.i64(i64 %a, i64 %b)
  %o = extractvalue {i64, i1} %r, 1
  ret i1 %o
}

; Only the 64-bit bitconvert has a pattern.  The 32-bit pair reaches
; MOVi2f_S/MOVf2i_S through custom lowering GlobalISel does not produce.
; CHECK-LABEL: bitcast_i32_f32:
; CHECK:       sts
define i32 @bitcast_i32_f32(float %x) {
  %r = bitcast float %x to i32
  ret i32 %r
}
