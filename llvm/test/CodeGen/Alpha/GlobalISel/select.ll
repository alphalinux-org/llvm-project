; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; cmovne leaves its destination alone when the condition is zero, so a select is
; the false value in the destination and a conditional move of the true one over
; it.  The operand order is the whole point, so pin it: a selector that swapped
; the two values would still emit a cmovne.

; An i1 condition is a low bit, which is what cmovlbs tests, so no separate
; mask is needed.  This is the pattern at AlphaInstrInfo.td's `select (and x,
; 1)'; it is reachable from GlobalISel only because the compare and the
; select's condition are both s64, and matches what SelectionDAG emits.
; CHECK-LABEL: sel_i64:
; CHECK:      bis $31, $18, $0
; CHECK-NEXT: cmovlbs $16, $17, $0
; CHECK-NEXT: ret
define i64 @sel_i64(i1 %c, i64 %a, i64 %b) {
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}

; A pointer-typed select does not reach the cmovlbs pattern, which is written
; on i64, so it still masks explicitly.  SelectionDAG emits cmovlbs here; the
; instruction count is the same either way.
; CHECK-LABEL: sel_ptr:
; CHECK:      bis $31, $18, $0
; CHECK-NEXT: and $16, 1, [[C:\$[0-9]+]]
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define ptr @sel_ptr(i1 %c, ptr %a, ptr %b) {
  %r = select i1 %c, ptr %a, ptr %b
  ret ptr %r
}

; A narrower select is widened to a whole register, so it selects with the same
; 64-bit cmovne.
; CHECK-LABEL: sel_i32:
; CHECK:      bis $31, $18, $0
; CHECK-NEXT: and $16, 1, [[C:\$[0-9]+]]
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define i32 @sel_i32(i1 %c, i32 %a, i32 %b) {
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; A comparison feeding the select is folded into the move itself: the compare
; result and the select's condition are both s64, so the imported cmovgt
; pattern matches and no separate compare is emitted at all.
; CHECK-LABEL: sel_cmp:
; CHECK:      bis $31, $18, $0
; CHECK-NEXT: cmovgt $16, $17, $0
; CHECK-NEXT: ret
define i64 @sel_cmp(i64 %x, i64 %a, i64 %b) {
  %c = icmp sgt i64 %x, 0
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}
