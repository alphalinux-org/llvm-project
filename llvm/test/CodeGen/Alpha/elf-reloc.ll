; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data -filetype=obj < %s \
; RUN:   | llvm-readobj -r - | FileCheck %s

; A call establishes the GP (gpdisp), loads the callee address from the GOT
; (literal) and reloads the GP afterwards (gpdisp).

; CHECK: R_ALPHA_GPDISP
; CHECK: R_ALPHA_LITERAL helper
; CHECK: R_ALPHA_GPDISP
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
