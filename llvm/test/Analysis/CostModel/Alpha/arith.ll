; RUN: opt -mtriple=alpha-unknown-linux-gnu -passes='print<cost-model>' \
; RUN:   -disable-output < %s 2>&1 | FileCheck %s

; The multiplier is slow and there is no divide instruction, so multiply and
; divide are costed well above the simple arithmetic they might be traded for.

; CHECK-LABEL: 'add'
; CHECK: cost of 1 for instruction:   %r = add i64
define i64 @add(i64 %a, i64 %b) {
  %r = add i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: 'mul'
; CHECK: cost of 4 for instruction:   %r = mul i64
define i64 @mul(i64 %a, i64 %b) {
  %r = mul i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: 'sdiv'
; CHECK: cost of 128 for instruction:   %r = sdiv i64
define i64 @sdiv(i64 %a, i64 %b) {
  %r = sdiv i64 %a, %b
  ret i64 %r
}

; CHECK-LABEL: 'urem'
; CHECK: cost of 128 for instruction:   %r = urem i64
define i64 @urem(i64 %a, i64 %b) {
  %r = urem i64 %a, %b
  ret i64 %r
}
