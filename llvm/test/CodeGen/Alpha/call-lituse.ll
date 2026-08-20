; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 \
; RUN:   -filetype=obj < %s | llvm-readobj -r - | FileCheck %s

; The same relocations have to survive the printed form, which is the only
; thing -S, -save-temps and an external assembler ever see.  The assembler's
; way of saying "this call uses that literal" is a sequence number on both, so
; the printed text carries one; without it the pair reassembles to a call the
; linker can never relax.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=pic -O2 < %s \
; RUN:   | llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o - \
; RUN:   | llvm-readobj -r - | FileCheck %s

; A direct call carries relocations on its jsr that let the linker optimize it.
; Both callees are tagged with lituse_jsr (addend 3) so the linker can relax the
; GOT-load-and-jsr into a direct bsr; a branch-prediction hint is emitted only
; for a non-local callee, matching gcc.  The lituse comes first, as GNU as
; writes it: bfd only inspects the relocation immediately after a literal's use
; and would not find a lituse hidden behind a hint.

; The lituse comes first, as GNU as writes it: bfd only inspects the relocation
; immediately after a literal's use and would not find a lituse hidden behind a
; hint, so the call would never be relaxed.

; The local call: a literal to load the callee's address and a lituse on the jsr
; that uses it, with the addend naming which use it is -- 3 is lituse_jsr, and a
; wrong one would have the linker relax the wrong instruction.  No hint.
; CHECK:      R_ALPHA_LITERAL loc 0x0
; CHECK-NEXT: R_ALPHA_LITUSE - 0x3
; CHECK-NOT:  R_ALPHA_HINT

; The external one carries the hint as well, on the same offset as the lituse.
; CHECK:      R_ALPHA_LITERAL ext 0x0
; CHECK-NEXT: [[EJSR:0x[0-9A-F]+]] R_ALPHA_LITUSE - 0x3
; CHECK-NEXT: [[EJSR]] R_ALPHA_HINT ext 0x0
; CHECK-NOT:  R_ALPHA_HINT

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
