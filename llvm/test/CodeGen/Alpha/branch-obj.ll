; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s

; A local branch resolved during object emission must land on the right
; instruction.  The Alpha displacement is measured from the instruction after
; the branch (PC+4), so a backward branch to the loop header five instructions
; earlier encodes -6, not -5.

; CHECK-LABEL: <loopcount>:
; CHECK:      bne {{\$[0-9]+}}, -6
; CHECK-NEXT: br {{\$[0-9]+}}, 0
define i64 @loopcount(i64 %n) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %i.next, %loop ]
  %acc = phi i64 [ 0, %entry ], [ %acc.next, %loop ]
  %acc.next = add i64 %acc, %i
  %i.next = add i64 %i, 1
  %c = icmp eq i64 %i.next, %n
  br i1 %c, label %exit, label %loop
exit:
  ret i64 %acc
}
