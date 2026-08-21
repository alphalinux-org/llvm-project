; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data -filetype=obj < %s \
; RUN:   | llvm-readobj -r - | FileCheck %s

; A call establishes the GP (gpdisp), loads the callee address from the GOT
; (literal) and reloads the GP afterwards (gpdisp).
;
; One R_ALPHA_GPDISP covers both halves of a pair, and its addend is the
; distance from the first instruction to the second -- 4 for an ldgp, and the
; linker needs it to find the lda it has to rewrite alongside the ldah.  A
; relocation type with no addend would say nothing about where the second half
; is, so the addend is checked rather than only the type.

; CHECK: R_ALPHA_GPDISP - 0x4
; CHECK: R_ALPHA_LITERAL helper
; CHECK: R_ALPHA_GPDISP - 0x4
declare i64 @helper(i64)
define i64 @call(i64 %x) {
  %r = call i64 @helper(i64 %x)
  ret i64 %r
}

; A small, locally-defined global goes in .sdata and is addressed GP-relative
; (gprelhigh/gprellow); a preemptible or external one stays in the GOT.

; CHECK: R_ALPHA_GPRELHIGH g
; CHECK: R_ALPHA_GPRELLOW g
@g = dso_local global i64 0
define i64 @loadg() {
  %v = load i64, ptr @g
  ret i64 %v
}
