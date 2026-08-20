; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; cmovne leaves its destination alone when the condition is zero, so a select is
; the false value in the destination and a conditional move of the true one over
; it.  The operand order is the whole point, so pin it: a selector that swapped
; the two values would still emit a cmovne.

; CHECK-LABEL: sel_i64:
; CHECK:      and $16, 1, [[C:\$[0-9]+]]
; CHECK-NEXT: bis $31, $18, $0
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define i64 @sel_i64(i1 %c, i64 %a, i64 %b) {
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}

; CHECK-LABEL: sel_ptr:
; CHECK:      and $16, 1, [[C:\$[0-9]+]]
; CHECK-NEXT: bis $31, $18, $0
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define ptr @sel_ptr(i1 %c, ptr %a, ptr %b) {
  %r = select i1 %c, ptr %a, ptr %b
  ret ptr %r
}

; A narrower select is widened to a whole register, so it selects with the same
; 64-bit cmovne.
; CHECK-LABEL: sel_i32:
; CHECK:      and $16, 1, [[C:\$[0-9]+]]
; CHECK-NEXT: bis $31, $18, $0
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define i32 @sel_i32(i1 %c, i32 %a, i32 %b) {
  %r = select i1 %c, i32 %a, i32 %b
  ret i32 %r
}

; A comparison feeding the select needs no masking: cmplt already produces 0 or
; 1.  The operands are reversed because sgt x, 0 is 0 < x.
; CHECK-LABEL: sel_cmp:
; CHECK:      bis $31, $18, $0
; CHECK-NEXT: lda [[Z:\$[0-9]+]], 0($31)
; CHECK-NEXT: cmplt [[Z]], $16, [[C:\$[0-9]+]]
; CHECK-NEXT: cmovne [[C]], $17, $0
; CHECK-NEXT: ret
define i64 @sel_cmp(i64 %x, i64 %a, i64 %b) {
  %c = icmp sgt i64 %x, 0
  %r = select i1 %c, i64 %a, i64 %b
  ret i64 %r
}
