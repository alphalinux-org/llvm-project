; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s \
; RUN:   | llvm-objdump -d - | FileCheck %s

; A local branch resolved during object emission must land on the right
; instruction.  The Alpha displacement is measured from the instruction after
; the branch (PC+4), so a backward branch to the loop header five instructions
; earlier encodes -6, not -5 -- which is the difference between landing on the
; header and landing one instruction past it.  The disassembler resolves the
; displacement against the branch's own address, so the target is checked here
; rather than the encoded number.

; CHECK-LABEL: <loopcount>:
; CHECK:      [[HDR:[0-9a-f]+]]: {{.*}}bis $31, $2, $0
; CHECK:      fa ff 7f f4 {{.*}}bne {{\$[0-9]+}}, 0x[[HDR]] <loopcount+0x[[HDR]]>
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
