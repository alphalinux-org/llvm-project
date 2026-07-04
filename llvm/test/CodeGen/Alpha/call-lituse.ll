; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 \
; RUN:   -filetype=obj < %s | llvm-readobj -r - | FileCheck %s

; A direct call carries relocations on its jsr that let the linker optimize it.
; A dso-local callee is tagged with only lituse_jsr (addend 3) so the linker can
; relax the GOT-load-and-jsr into a direct bsr; a branch-prediction hint, which
; would inhibit that relaxation, is emitted only for a non-local callee (which
; cannot be relaxed anyway).

; The local callee produces a lituse_jsr but no hint; the external callee
; produces both.  So exactly one R_ALPHA_HINT and two R_ALPHA_LITUSE appear.

; CHECK-NOT: R_ALPHA_HINT
; CHECK:     R_ALPHA_LITUSE
; CHECK:     R_ALPHA_HINT ext
; CHECK:     R_ALPHA_LITUSE
; CHECK-NOT: R_ALPHA_HINT

declare dso_local i32 @loc(i32)
declare i32 @ext(i32)

define i32 @call_local(i32 %x) {
  %r = call i32 @loc(i32 %x)
  ret i32 %r
}

define i32 @call_external(i32 %x) {
  %r = call i32 @ext(i32 %x)
  ret i32 %r
}
