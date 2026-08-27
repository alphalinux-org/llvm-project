; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; CHECK-LABEL: eq0:
; CHECK:      cmpeq $16, 0, $0
; CHECK-NEXT: ret
define i64 @eq0(i64 %x) {
  %c = icmp eq i64 %x, 0
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ne0:
; CHECK:      cmpeq $16, 0, $0
; CHECK-NEXT: xor $0, 1, $0
; CHECK-NEXT: ret
define i64 @ne0(i64 %x) {
  %c = icmp ne i64 %x, 0
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: slt10:
; CHECK:      cmplt $16, 10, $0
; CHECK-NEXT: ret
define i64 @slt10(i64 %x) {
  %c = icmp slt i64 %x, 10
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ult5:
; CHECK:      cmpult $16, 5, $0
; CHECK-NEXT: ret
define i64 @ult5(i64 %x) {
  %c = icmp ult i64 %x, 5
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: eq7:
; CHECK:      cmpeq $16, 7, $0
; CHECK-NEXT: ret
define i64 @eq7(i64 %x) {
  %c = icmp eq i64 %x, 7
  %r = zext i1 %c to i64
  ret i64 %r
}

; Greater-than against zero swaps operands; the zero uses the zero register.
; CHECK-LABEL: sgt0:
; CHECK:      cmplt $31, $16, $0
; CHECK-NEXT: ret
define i64 @sgt0(i64 %x) {
  %c = icmp sgt i64 %x, 0
  %r = zext i1 %c to i64
  ret i64 %r
}

; `x <= C' is canonicalized to `x < C+1', which costs nothing until C is 255:
; 256 does not fit the 8-bit literal field, so the form that needs no constant
; is the one that has to build one.  cmple takes the literal back.  255 is the
; only constant where the two forms differ -- 257 fits neither.
; CHECK-LABEL: sle255:
; CHECK-NOT:  lda
; CHECK:      cmple $16, 255, $0
; CHECK-NEXT: ret
define i64 @sle255(i64 %x) {
  %c = icmp sle i64 %x, 255
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ule255:
; CHECK-NOT:  lda
; CHECK:      cmpule $16, 255, $0
; CHECK-NEXT: ret
define i64 @ule255(i64 %x) {
  %c = icmp ule i64 %x, 255
  %r = zext i1 %c to i64
  ret i64 %r
}

; One below the boundary still goes the canonical way, and must: cmplt with 255
; is the same one instruction, so rewriting it would be churn.
; CHECK-LABEL: sle254:
; CHECK:      cmplt $16, 255, $0
; CHECK-NEXT: ret
define i64 @sle254(i64 %x) {
  %c = icmp sle i64 %x, 254
  %r = zext i1 %c to i64
  ret i64 %r
}
