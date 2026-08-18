; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: slt:
; CHECK-NOT:   cmp
; CHECK:       cmovlt $16, $17, $0
; CHECK:       ret
define i64 @slt(i64 %a, i64 %t, i64 %f) {
  %c = icmp slt i64 %a, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; CHECK-LABEL: seq:
; CHECK-NOT:   cmp
; CHECK:       cmoveq $16, $17, $0
; CHECK:       ret
define i64 @seq(i64 %a, i64 %t, i64 %f) {
  %c = icmp eq i64 %a, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

; CHECK-LABEL: sgt:
; CHECK:       cmovgt $16, $17, $0
; CHECK:       ret
define i64 @sgt(i64 %a, i64 %t, i64 %f) {
  %c = icmp sgt i64 %a, 0
  %r = select i1 %c, i64 %t, i64 %f
  ret i64 %r
}

