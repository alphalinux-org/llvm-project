; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Integer comparisons.  Alpha has no condition-code register, so a comparison
; writes 0 or 1 into a general register.  Conditions without a direct
; instruction are formed by swapping the operands or inverting equality.

; CHECK-LABEL: eq:
; CHECK:       cmpeq $16, $17, $0
; CHECK-NEXT:  ret
define i64 @eq(i64 %a, i64 %b) {
  %c = icmp eq i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ne:
; CHECK:       cmpeq $16, $17, $0
; CHECK:       xor $0,
; CHECK:       ret
define i64 @ne(i64 %a, i64 %b) {
  %c = icmp ne i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: slt:
; CHECK:       cmplt $16, $17, $0
; CHECK-NEXT:  ret
define i64 @slt(i64 %a, i64 %b) {
  %c = icmp slt i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; sgt is slt with the operands swapped.
; CHECK-LABEL: sgt:
; CHECK:       cmplt $17, $16, $0
; CHECK-NEXT:  ret
define i64 @sgt(i64 %a, i64 %b) {
  %c = icmp sgt i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: sle:
; CHECK:       cmple $16, $17, $0
; CHECK-NEXT:  ret
define i64 @sle(i64 %a, i64 %b) {
  %c = icmp sle i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ult:
; CHECK:       cmpult $16, $17, $0
; CHECK-NEXT:  ret
define i64 @ult(i64 %a, i64 %b) {
  %c = icmp ult i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ule:
; CHECK:       cmpule $16, $17, $0
; CHECK-NEXT:  ret
define i64 @ule(i64 %a, i64 %b) {
  %c = icmp ule i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; The last three are swaps too, exactly like sgt above: a >= b is b <= a, and
; unsigned a > b is b < a.  A swap computes the condition itself rather than
; its complement, so none of these needs an xor; ne is the one that does,
; because inverting equality is what no swap reaches.
; CHECK-LABEL: sge:
; CHECK:       cmple $17, $16, $0
; CHECK-NEXT:  ret
define i64 @sge(i64 %a, i64 %b) {
  %c = icmp sge i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ugt:
; CHECK:       cmpult $17, $16, $0
; CHECK-NEXT:  ret
define i64 @ugt(i64 %a, i64 %b) {
  %c = icmp ugt i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: uge:
; CHECK:       cmpule $17, $16, $0
; CHECK-NEXT:  ret
define i64 @uge(i64 %a, i64 %b) {
  %c = icmp uge i64 %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}
