; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: diamond:
; CHECK:       bne $0,
; CHECK:       ret
define i64 @diamond(i64 %a, i64 %b) {
entry:
  %c = icmp slt i64 %a, %b
  br i1 %c, label %t, label %f
t:
  ret i64 %a
f:
  ret i64 %b
}

; A loop exercises an unconditional back-edge branch.
; CHECK-LABEL: countdown:
; CHECK:       bne $
; CHECK:       ret
define i64 @countdown(i64 %n) {
entry:
  br label %loop
loop:
  %i = phi i64 [ %n, %entry ], [ %next, %loop ]
  %next = add i64 %i, -1
  %done = icmp eq i64 %next, 0
  br i1 %done, label %exit, label %loop
exit:
  ret i64 %next
}
