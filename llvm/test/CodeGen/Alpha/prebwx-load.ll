; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s

; Without BWX, a byte or word load reads the aligned quadword with ldq_u and
; extracts the datum at the position given by the low address bits with
; extbl/extwl (zero-extended).

; CHECK-LABEL: loadi8:
; CHECK:       ldq_u $0, 0($16)
; CHECK-NEXT:  extbl $0, $16, $0
; CHECK-NEXT:  ret
define i64 @loadi8(ptr %p) {
  %v = load i8, ptr %p
  %r = zext i8 %v to i64
  ret i64 %r
}

; CHECK-LABEL: loadi16:
; CHECK:       ldq_u $0, 0($16)
; CHECK-NEXT:  extwl $0, $16, $0
; CHECK-NEXT:  ret
define i64 @loadi16(ptr %p) {
  %v = load i16, ptr %p
  %r = zext i16 %v to i64
  ret i64 %r
}

; A signed byte load extends the extracted value.
; CHECK-LABEL: loadi8_sext:
; CHECK:       ldq_u $0, 0($16)
; CHECK:       extbl $0, $16, $0
; CHECK:       sll $0, 56, $0
; CHECK:       sra $0, 56, $0
; CHECK:       ret
define i64 @loadi8_sext(ptr %p) {
  %v = load i8, ptr %p
  %r = sext i8 %v to i64
  ret i64 %r
}
