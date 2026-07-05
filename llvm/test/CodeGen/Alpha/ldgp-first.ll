; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev56 < %s | FileCheck %s

; The entry ldgp must be the first instruction: its !gpdisp relocation resolves
; relative to the ldah's address, which equals the incoming procedure value
; ($27) only when nothing precedes it.  The post-RA scheduler must not hoist an
; independent instruction (here the key-'A' subtract) ahead of it.

@tab = external global [64 x i64]

; CHECK-LABEL: key2index:
; The ldgp has to be the *first* instruction, so match it from the start of the
; entry block rather than anywhere in the function: a plain CHECK would be
; satisfied by an ldgp the scheduler had sunk below other work.
; CHECK:      %bb.0:
; CHECK-NEXT: ldgp $29, 0($27)
; CHECK-NOT:  ldgp
define i64 @key2index(i64 %key) {
entry:
  %sub = add i64 %key, -65
  %cmp = icmp ult i64 %sub, 26
  br i1 %cmp, label %hit, label %miss
hit:
  %add = add i64 %sub, 10
  ret i64 %add
miss:
  %idx = and i64 %key, 63
  %p = getelementptr [64 x i64], ptr @tab, i64 0, i64 %idx
  %v = load i64, ptr %p
  ret i64 %v
}
