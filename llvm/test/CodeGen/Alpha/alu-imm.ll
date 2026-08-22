; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: addi:
; CHECK:       addq $16, 5, $0
; CHECK-NEXT:  ret
define i64 @addi(i64 %x) {
  %r = add i64 %x, 5
  ret i64 %r
}

; CHECK-LABEL: shli:
; CHECK:       sll $16, 3, $0
; CHECK-NEXT:  ret
define i64 @shli(i64 %x) {
  %r = shl i64 %x, 3
  ret i64 %r
}

; CHECK-LABEL: ori:
; CHECK:       bis $16, 8, $0
; CHECK-NEXT:  ret
define i64 @ori(i64 %x) {
  %r = or i64 %x, 8
  ret i64 %r
}

; CHECK-LABEL: andi:
; CHECK:       and $16, 12, $0
; CHECK-NEXT:  ret
define i64 @andi(i64 %x) {
  %r = and i64 %x, 12
  ret i64 %r
}

; A constant too wide for the 8-bit operate literal still fits lda's 16-bit
; displacement, and lda computes $Rb + sext16(disp) with no side effects -- so
; it is the add, not a materialization to be added afterwards.
; CHECK-LABEL: addbig:
; CHECK:       lda $0, 1000($16)
; CHECK-NEXT:  ret
define i64 @addbig(i64 %x) {
  %r = add i64 %x, 1000
  ret i64 %r
}

; A constant subtract reaches instruction selection as an add of the negation,
; which DAGCombine has already done, so it takes the same one instruction.
; CHECK-LABEL: subbig:
; CHECK:       lda $0, -5($16)
; CHECK-NEXT:  ret
define i64 @subbig(i64 %x) {
  %r = sub i64 %x, 5
  ret i64 %r
}

; Wider than that, the constant is materialized and added in register form.
; CHECK-LABEL: addhuge:
; CHECK:       ldah $0, 1($31)
; CHECK:       lda $0, 4464($0)
; CHECK:       addq $16, $0, $0
define i64 @addhuge(i64 %x) {
  %r = add i64 %x, 70000
  ret i64 %r
}
