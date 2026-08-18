; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: smax:
; CHECK:      cmplt $0, $16, $1
; CHECK-NEXT: cmovne $1, $16, $0
; CHECK-NEXT: ret
define i64 @smax(i64 %a, i64 %b) {
  %r = call i64 @llvm.smax.i64(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: smin:
; CHECK:      cmplt $16, $0, $1
; CHECK-NEXT: cmovne $1, $16, $0
; CHECK-NEXT: ret
define i64 @smin(i64 %a, i64 %b) {
  %r = call i64 @llvm.smin.i64(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: umax:
; CHECK:      cmpult $0, $16, $1
; CHECK-NEXT: cmovne $1, $16, $0
; CHECK-NEXT: ret
define i64 @umax(i64 %a, i64 %b) {
  %r = call i64 @llvm.umax.i64(i64 %a, i64 %b)
  ret i64 %r
}

; CHECK-LABEL: umin:
; CHECK:      cmpult $16, $0, $1
; CHECK-NEXT: cmovne $1, $16, $0
; CHECK-NEXT: ret
define i64 @umin(i64 %a, i64 %b) {
  %r = call i64 @llvm.umin.i64(i64 %a, i64 %b)
  ret i64 %r
}

; abs is (x ^ (x >> 63)) - (x >> 63), a branchless shift/xor/subtract.
; CHECK-LABEL: absv:
; CHECK-NOT:  cmov
; CHECK:      sra $16, 63, $0
; CHECK-NEXT: xor $16, $0, $1
; CHECK-NEXT: subq $1, $0, $0
; CHECK-NEXT: ret
define i64 @absv(i64 %a) {
  %r = call i64 @llvm.abs.i64(i64 %a, i1 false)
  ret i64 %r
}

declare i64 @llvm.smax.i64(i64, i64)
declare i64 @llvm.smin.i64(i64, i64)
declare i64 @llvm.umax.i64(i64, i64)
declare i64 @llvm.umin.i64(i64, i64)
declare i64 @llvm.abs.i64(i64, i1)
