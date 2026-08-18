; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

target datalayout = "e-m:e-p:64:64-i64:64-i128:128-n64-S128"

; CHECK:      .globl  word
; CHECK:      .p2align 2
; CHECK-LABEL: word:
; CHECK-NEXT:   .long 7
; CHECK-NEXT:   .size word, 4
@word = global i32 7

; A 64-bit pointer is emitted little-endian as a .quad.
; CHECK:      .globl  ptr
; CHECK:      .p2align 3
; CHECK-LABEL: ptr:
; CHECK-NEXT:   .quad 0
; CHECK-NEXT:   .size ptr, 8
@ptr = global ptr null

; CHECK:      .globl  quad
; CHECK-LABEL: quad:
; CHECK-NEXT:   .quad 1311768467294899696
@quad = global i64 u0x1234567890ABCDF0
