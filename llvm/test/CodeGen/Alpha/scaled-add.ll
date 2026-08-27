; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Quadword scaled add/subtract, and the longword forms that fold the sign-extend.
define i64 @q4(i64 %a, i64 %b) {
; CHECK-LABEL: q4:
; CHECK: s4addq $16, $17, $0
  %s = shl i64 %a, 2
  %r = add i64 %s, %b
  ret i64 %r
}

; The quadword subtract, which no other case here reaches.
define i64 @q4sub(i64 %a, i64 %b) {
; CHECK-LABEL: q4sub:
; CHECK: s4subq $16, $17, $0
  %s = shl i64 %a, 2
  %r = sub i64 %s, %b
  ret i64 %r
}
define signext i32 @l4(i32 %a, i32 %b) {
; CHECK-LABEL: l4:
; CHECK: s4addl $16, $17, $0
; CHECK-NOT: addl $0, $31
  %s = shl i32 %a, 2
  %r = add i32 %s, %b
  ret i32 %r
}
define signext i32 @l8sub(i32 %a, i32 %b) {
; CHECK-LABEL: l8sub:
; CHECK: s8subl $16, $17, $0
  %s = shl i32 %a, 3
  %r = sub i32 %s, %b
  ret i32 %r
}

; A literal addend is an operand, not a separate constant: an operate
; instruction takes an 8-bit literal wherever it takes $Rb, and array address
; arithmetic reaches this constantly.
define i64 @q4_lit(i64 %a) {
; CHECK-LABEL: q4_lit:
; CHECK-NOT: lda
; CHECK: s4addq $16, 7, $0
  %s = shl i64 %a, 2
  %r = add i64 %s, 7
  ret i64 %r
}

; The addend is smaller than the scale here, so DAGCombine proves the operands
; share no bits and rewrites the add as an or.  It is the same instruction.
define i64 @q8_or(i64 %a) {
; CHECK-LABEL: q8_or:
; CHECK-NOT: sll
; CHECK: s8addq $16, 7, $0
  %s = shl i64 %a, 3
  %r = add i64 %s, 7
  ret i64 %r
}

; A subtract arrives as an add of a negative constant, which is neither a
; literal nor an lda displacement here; s4subq takes it back.
define i64 @q4_sub_lit(i64 %a) {
; CHECK-LABEL: q4_sub_lit:
; CHECK-NOT: lda
; CHECK: s4subq $16, 7, $0
  %s = shl i64 %a, 2
  %r = sub i64 %s, 7
  ret i64 %r
}

; The longword forms fold the sign-extend as well as the literal.
define signext i32 @l8_lit(i32 signext %a) {
; CHECK-LABEL: l8_lit:
; CHECK-NOT: lda
; CHECK: s8addl $16, 9, $0
  %s = shl i32 %a, 3
  %r = add i32 %s, 9
  ret i32 %r
}

define signext i32 @l4_sub_lit(i32 signext %a) {
; CHECK-LABEL: l4_sub_lit:
; CHECK-NOT: lda
; CHECK: s4subl $16, 9, $0
  %s = shl i32 %a, 2
  %r = sub i32 %s, 9
  ret i32 %r
}
