; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; A comparison against zero becomes a single test-and-branch.  The branch is
; emitted for the fall-through edge, so the condition is the inverse of the
; source predicate.

declare void @sink()

; x < 0 : branch to the join block when x >= 0.
; CHECK-LABEL: lt:
; CHECK:      bge $16,
; CHECK-NOT:  cmplt
define void @lt(i64 %x) {
  %c = icmp slt i64 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x >= 0 : branch when x < 0.
; CHECK-LABEL: ge:
; CHECK:      blt $16,
define void @ge(i64 %x) {
  %c = icmp sge i64 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x > 0 : branch when x <= 0.
; CHECK-LABEL: gt:
; CHECK:      ble $16,
define void @gt(i64 %x) {
  %c = icmp sgt i64 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x == 0 : branch when nonzero.
; CHECK-LABEL: eqz:
; CHECK:      bne $16,
define void @eqz(i64 %x) {
  %c = icmp eq i64 %x, 0
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; A low-bit test folds the mask into blbc/blbs.
; CHECK-LABEL: lowbit:
; CHECK:      blbc $16,
; CHECK-NOT:  and
define void @lowbit(i64 %x) {
  %a = and i64 %x, 1
  %c = icmp ne i64 %a, 0
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; Comparing two variables still needs an explicit compare feeding a bne.
; CHECK-LABEL: cmpvar:
; CHECK:      cmp{{l[te]}} $1{{[67]}}, $1{{[67]}},
; CHECK:      bne
define void @cmpvar(i64 %x, i64 %y) {
  %c = icmp slt i64 %x, %y
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; The comparison a branch is folded into is not always against zero.  The
; middle end turns a relation against zero into a strict one against 1 or -1 --
; x >= 0 becomes x > -1, x > 0 becomes x >= 1 -- so those forms reach here too
; and fold to the same branches.

; x >= 1 is x > 0, so the branch over the call is its inverse.
; CHECK-LABEL: ge_one:
; CHECK:      ble $16,
define void @ge_one(i64 %x) {
  %c = icmp sge i64 %x, 1
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; And taken the other way round it is the branch itself.
; CHECK-LABEL: ge_one_taken:
; CHECK:      bgt $16,
define void @ge_one_taken(i64 %x) {
  %c = icmp sge i64 %x, 1
  br i1 %c, label %f, label %t
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x < 1 is x <= 0.
; CHECK-LABEL: lt_one:
; CHECK:      bgt $16,
define void @lt_one(i64 %x) {
  %c = icmp slt i64 %x, 1
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x > -1 is x >= 0.
; CHECK-LABEL: gt_minus_one:
; CHECK:      blt $16,
define void @gt_minus_one(i64 %x) {
  %c = icmp sgt i64 %x, -1
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; x <= -1 is x < 0.
; CHECK-LABEL: le_minus_one:
; CHECK:      bge $16,
define void @le_minus_one(i64 %x) {
  %c = icmp sle i64 %x, -1
  br i1 %c, label %t, label %f
t:
  call void @sink()
  br label %f
f:
  ret void
}

; blbs is the other half of the low-bit fold.
; CHECK-LABEL: low_bit_set:
; CHECK:      blbs $16,
define void @low_bit_set(i64 %a) {
  %m = and i64 %a, 1
  %c = icmp ne i64 %m, 0
  br i1 %c, label %f, label %t
t:
  call void @sink()
  br label %f
f:
  ret void
}
