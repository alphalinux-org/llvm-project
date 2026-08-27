; The low-bit conditional moves.  `(x & 1) ? a : b` is one instruction on Alpha:
; cmovlbs moves when bit 0 of its first operand is set, cmovlbc when it is
; clear, so the `and $x, 1` an ordinary cmovne would need is not emitted at all.
; The CHECK-NOT is the whole point of each case -- the instruction being right
; and the mask still being there would be no better than the mask alone.

; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A select whose condition is the low bit itself, with no compare.
; CHECK-LABEL: lowbit:
; CHECK-NOT:   and $16,
; CHECK:       cmovlbs $16, $17, $0
define i64 @lowbit(i64 %x, i64 %t, i64 %f) {
  %m = and i64 %x, 1
  %b = trunc i64 %m to i1
  %r = select i1 %b, i64 %t, i64 %f
  ret i64 %r
}

; The same written as a compare against zero, which is what C's `(x & 1) ? :`
; becomes.
; CHECK-LABEL: setne:
; CHECK-NOT:   and $16,
; CHECK:       cmovlbs $16, $17, $0
define i64 @setne(i64 %x, i64 %t, i64 %f) {
  %m = and i64 %x, 1
  %c = icmp ne i64 %m, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; The inverted condition is the other instruction, not an extra one.
; CHECK-LABEL: seteq:
; CHECK-NOT:   and $16,
; CHECK:       cmovlbc $16, $17, $0
define i64 @seteq(i64 %x, i64 %t, i64 %f) {
  %m = and i64 %x, 1
  %c = icmp eq i64 %m, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; A mask that is not 1 is a different question and must keep its `and`: cmovlb
; reads bit 0, and bit 1 is not bit 0.
; CHECK-LABEL: bit1:
; CHECK:       and $16, 2, [[C:\$[0-9]+]]
; CHECK:       cmovne [[C]], $17, $0
define i64 @bit1(i64 %x, i64 %t, i64 %f) {
  %m = and i64 %x, 2
  %c = icmp ne i64 %m, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}
