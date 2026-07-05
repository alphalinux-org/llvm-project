; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s
; decomposeMulByConstant does not consult the subtarget, so the decomposition
; is the same on an in-order core; run one to keep that true.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s

; x * 3 = 4x - x.
; CHECK-LABEL: mul3:
; CHECK:      s4subq $16, $16, $0
; CHECK-NEXT: ret
define i64 @mul3(i64 %a) {
  %r = mul i64 %a, 3
  ret i64 %r
}

; x * 5 = 4x + x.
; CHECK-LABEL: mul5:
; CHECK:      s4addq $16, $16, $0
; CHECK-NEXT: ret
define i64 @mul5(i64 %a) {
  %r = mul i64 %a, 5
  ret i64 %r
}

; x * 7 = 8x - x.
; CHECK-LABEL: mul7:
; CHECK:      s8subq $16, $16, $0
; CHECK-NEXT: ret
define i64 @mul7(i64 %a) {
  %r = mul i64 %a, 7
  ret i64 %r
}

; x * 9 = 8x + x.
; CHECK-LABEL: mul9:
; CHECK:      s8addq $16, $16, $0
; CHECK-NEXT: ret
define i64 @mul9(i64 %a) {
  %r = mul i64 %a, 9
  ret i64 %r
}

; x * 15 = (x << 4) - x, cheaper than the multiplier.
; CHECK-LABEL: mul15:
; CHECK:      sll $16, 4, $0
; CHECK-NEXT: subq $0, $16, $0
; CHECK-NEXT: ret
define i64 @mul15(i64 %a) {
  %r = mul i64 %a, 15
  ret i64 %r
}

; x * 17 = (x << 4) + x.
; CHECK-LABEL: mul17:
; CHECK:      sll $16, 4, $0
; CHECK-NEXT: addq $0, $16, $0
; CHECK-NEXT: ret
define i64 @mul17(i64 %a) {
  %r = mul i64 %a, 17
  ret i64 %r
}

; A constant that is not near a power of two stays a multiply.
; CHECK-LABEL: mul11:
; CHECK:      mulq $16, 11, $0
; CHECK-NEXT: ret
define i64 @mul11(i64 %a) {
  %r = mul i64 %a, 11
  ret i64 %r
}

; 25 = 5 * 5 factors into two scaled adds instead of a multiply.
; CHECK-LABEL: mul25:
; CHECK-NOT:  mulq
; CHECK:      s4addq $16, $16, $0
; CHECK-NEXT: s4addq $0, $0, $0
; CHECK-NEXT: ret
define i64 @mul25(i64 %a) {
  %r = mul i64 %a, 25
  ret i64 %r
}

; 27 = 9 * 3 peels the other two factors: s8addq for the 9, s4subq for the 3.
; CHECK-LABEL: mul27:
; CHECK-NOT:  mulq
; CHECK:      s8addq $16, $16, $0
; CHECK-NEXT: s4subq $0, $0, $0
; CHECK-NEXT: ret
define i64 @mul27(i64 %a) {
  %r = mul i64 %a, 27
  ret i64 %r
}

; 45 = 9 * 5 mixes the two adds.
; CHECK-LABEL: mul45:
; CHECK-NOT:  mulq
; CHECK:      s8addq $16, $16, $0
; CHECK-NEXT: s4addq $0, $0, $0
; CHECK-NEXT: ret
define i64 @mul45(i64 %a) {
  %r = mul i64 %a, 45
  ret i64 %r
}

; 100 = 4 * 25: still a short shift/add chain, no multiply.
; CHECK-LABEL: mul100:
; CHECK-NOT:  mulq
; CHECK:      ret
define i64 @mul100(i64 %a) {
  %r = mul i64 %a, 100
  ret i64 %r
}

; A constant whose shift/add chain would be longer than the multiplier keeps
; the multiply (10000 = 2^4 * 5^4).
; CHECK-LABEL: mul10000:
; CHECK:      mulq
define i64 @mul10000(i64 %a) {
  %r = mul i64 %a, 10000
  ret i64 %r
}

; The longword forms fold the sign-extension: i32 x * 3 = s4subl.
; CHECK-LABEL: mul3_32:
; CHECK:      s4subl $16, $16, $0
; CHECK-NEXT: ret
define signext i32 @mul3_32(i32 %a) {
  %r = mul i32 %a, 3
  ret i32 %r
}

; i32 x * 5 = s4addl.
; CHECK-LABEL: mul5_32:
; CHECK:      s4addl $16, $16, $0
; CHECK-NEXT: ret
define signext i32 @mul5_32(i32 %a) {
  %r = mul i32 %a, 5
  ret i32 %r
}

; x * -5 = -(4x + x).
; CHECK-LABEL: muln5:
; CHECK:      s4addq $16, $16, $0
; CHECK-NEXT: subq $31, $0, $0
; CHECK-NEXT: ret
define i64 @muln5(i64 %a) {
  %r = mul i64 %a, -5
  ret i64 %r
}

; The factoring path takes the constant's magnitude and negates the result, as
; the generic decomposition one commit earlier already does.  Taking the
; constant zero-extended would leave a negative one astronomically large, its
; estimated chain past the cut-off, and the multiply for the multiplier.
; CHECK-LABEL: muln25:
; CHECK:      s4addq $16, $16, $0
; CHECK-NEXT: s4addq $0, $0, $0
; CHECK-NEXT: subq $31, $0, $0
; CHECK-NEXT: ret
define i64 @muln25(i64 %a) {
  %r = mul i64 %a, -25
  ret i64 %r
}

; CHECK-LABEL: muln100:
; CHECK-NOT:  mulq
; CHECK:      subq $31, $0, $0
; CHECK-NEXT: ret
define i64 @muln100(i64 %a) {
  %r = mul i64 %a, -100
  ret i64 %r
}
