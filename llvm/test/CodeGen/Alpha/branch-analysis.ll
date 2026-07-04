; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; Branch analysis (analyzeBranch / insertBranch / removeBranch) lets the branch
; folder tail-merge blocks and drop redundant branches.

declare void @boom() noreturn

; The two identical abort blocks are tail-merged: both branches target the same
; block, and only one call to the abort routine remains.
; CHECK-LABEL: two_asserts:
; CHECK:       bne $16, [[ABORT:\.LBB[0-9_]+]]
; CHECK:       beq {{\$[0-9]+}}, [[ABORT]]
; CHECK:       ret
; CHECK:      [[ABORT]]:
; CHECK:       jsr $26, ($27)
; CHECK-NOT:   jsr $26, ($27)
define i64 @two_asserts(i64 %x, i64 %y) {
  %c1 = icmp eq i64 %x, 0
  br i1 %c1, label %ok1, label %bad1
bad1:
  call void @boom()
  unreachable
ok1:
  %c2 = icmp ult i64 %y, 100
  br i1 %c2, label %ok2, label %bad2
bad2:
  call void @boom()
  unreachable
ok2:
  %r = add i64 %x, %y
  ret i64 %r
}

; reverseBranchCondition is what lets the loop latch branch back on the inverted
; condition instead of branching over an unconditional branch.  Without it the
; latch ends `bne' to the exit followed by a `br' to the header.
; CHECK-LABEL: loop:
; CHECK:       bne {{\$[0-9]+}}, [[EXIT:\.LBB[0-9_]+]]
; CHECK:       beq {{\$[0-9]+}}, [[HEAD:\.LBB[0-9_]+]]
; CHECK-NOT:   br $31,
; CHECK:      [[EXIT]]:
declare void @sink(i64)
define void @loop(ptr %p, i64 %n) {
entry:
  br label %head
head:
  %i = phi i64 [0, %entry], [%next, %body]
  %c = icmp slt i64 %i, %n
  br i1 %c, label %body, label %exit
body:
  %v = load i64, ptr %p
  call void @sink(i64 %v)
  %next = add i64 %i, 1
  br label %head
exit:
  ret void
}
