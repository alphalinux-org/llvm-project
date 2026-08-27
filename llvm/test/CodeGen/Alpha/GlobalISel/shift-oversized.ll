; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=2 \
; RUN:   < %s -o /dev/null 2>&1 | FileCheck %s

; A shift of a scalar wider than a quadword.  clampScalar splits such a type in
; half, which is only a legalization when the width is a whole number of
; quadwords: half of an i120 is an s60, and legalizing an s60 goes nowhere
; good.  The legalizer refuses the ones that are not, and the function falls
; back to SelectionDAG, which expands the shift itself.
;
; If that refusal is removed, @lshr96 fails quickly, with `unable to legalize
; %N:_(s48) = G_MERGE_VALUES` from the halves of the i96.  @lshr120 does not
; fail at all: 120 and 8 have only 4 in common, so the artifact combiner spends
; millions of virtual registers unmerging s60s into s4s, and this test hangs
; rather than failing.  A hang here means this rule.  Two csmith programs in
; the E/gisel census stopped compiling on exactly that once the loads above
; them became legal enough to reach it.
;
; Rounding the width up to the next power of two instead -- the rule the G_ADD
; family carries -- fixes those two programs and hangs legalize.mir's @add_s65,
; where an s65 widened to s128 sends the legalizer and the artifact combiner
; passing an unmerge of a quadword into 64 booleans back and forth forever.
;
; The values go through memory because the translator cannot pass one of these
; in registers.

; @lshr128 is the other half of the rule: a width that *is* a whole number of
; quadwords must still be legalized here, not handed away with the rest.
; CHECK-NOT: fallback path for lshr128
define void @lshr128(ptr %p, ptr %q) {
  %a = load i128, ptr %p, align 16
  %s = lshr i128 %a, 37
  store i128 %s, ptr %q, align 16
  ret void
}

; CHECK: fallback path for lshr120
define void @lshr120(ptr %p, ptr %q) {
  %a = load i120, ptr %p, align 16
  %s = lshr i120 %a, 37
  store i120 %s, ptr %q, align 16
  ret void
}

; CHECK: fallback path for lshr96
define void @lshr96(ptr %p, ptr %q) {
  %a = load i96, ptr %p, align 16
  %s = lshr i96 %a, 37
  store i96 %s, ptr %q, align 16
  ret void
}
