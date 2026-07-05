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
