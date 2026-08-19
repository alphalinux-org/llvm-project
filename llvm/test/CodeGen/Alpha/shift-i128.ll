; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; There is no instruction that shifts a register pair, so a variable i128 shift
; becomes a libcall and a constant one is expanded inline.

define i128 @shl_i128(i128 %a, i128 %b) {
; CHECK-LABEL: shl_i128:
; CHECK: __ashlti3
  %r = shl i128 %a, %b
  ret i128 %r
}

define i128 @ashr_i128(i128 %a, i128 %b) {
; CHECK-LABEL: ashr_i128:
; CHECK: __ashrti3
  %r = ashr i128 %a, %b
  ret i128 %r
}

define i128 @lshr_i128(i128 %a, i128 %b) {
; CHECK-LABEL: lshr_i128:
; CHECK: __lshrti3
  %r = lshr i128 %a, %b
  ret i128 %r
}

; A 128-bit value is returned in memory, so the halves are stored through the
; buffer pointer the caller passed in $16.
define i128 @ashr_i128_const(i128 %a) {
; CHECK-LABEL: ashr_i128_const:
; CHECK-DAG:  sra $18, 17, {{\$[0-9]+}}
; CHECK-DAG:  sll $18, 47, {{\$[0-9]+}}
; CHECK-DAG:  srl $17, 17, {{\$[0-9]+}}
; CHECK-NOT:  __ashrti3
  %r = ashr i128 %a, 17
  ret i128 %r
}
