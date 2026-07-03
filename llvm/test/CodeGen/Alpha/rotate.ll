; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; Alpha has no rotate instruction, so a rotate becomes a pair of shifts ored
; together.

declare i64 @llvm.fshl.i64(i64, i64, i64)
declare i64 @llvm.fshr.i64(i64, i64, i64)

; A constant count needs no masking: the two shift amounts are known and add up
; to 64.
; CHECK-LABEL: rotl_const:
; CHECK-DAG:  srl $16, 56, $[[A:[0-9]+]]
; CHECK-DAG:  sll $16, 8, $[[B:[0-9]+]]
; CHECK:      bis
; CHECK-NEXT: ret
define i64 @rotl_const(i64 %x) {
  %r = call i64 @llvm.fshl.i64(i64 %x, i64 %x, i64 8)
  ret i64 %r
}

; CHECK-LABEL: rotr_const:
; CHECK-DAG:  srl $16, 8, $[[A:[0-9]+]]
; CHECK-DAG:  sll $16, 56, $[[B:[0-9]+]]
; CHECK:      bis
; CHECK-NEXT: ret
define i64 @rotr_const(i64 %x) {
  %r = call i64 @llvm.fshr.i64(i64 %x, i64 %x, i64 8)
  ret i64 %r
}

; A variable count is masked to 63 on both sides, and the second amount is the
; negation of the first.  Alpha's shifts already take their count modulo 64, but
; the negation has to be masked or a count of zero shifts by 64 rather than 0.
; CHECK-LABEL: rotl_var:
; CHECK-DAG:  and $17, 63,
; CHECK-DAG:  subq {{\$[0-9]+}}, $17,
; CHECK:      bis
; CHECK-NEXT: ret
define i64 @rotl_var(i64 %x, i64 %n) {
  %r = call i64 @llvm.fshl.i64(i64 %x, i64 %x, i64 %n)
  ret i64 %r
}

; CHECK-LABEL: rotr_var:
; CHECK-DAG:  and $17, 63,
; CHECK-DAG:  subq {{\$[0-9]+}}, $17,
; CHECK:      bis
; CHECK-NEXT: ret
define i64 @rotr_var(i64 %x, i64 %n) {
  %r = call i64 @llvm.fshr.i64(i64 %x, i64 %x, i64 %n)
  ret i64 %r
}
