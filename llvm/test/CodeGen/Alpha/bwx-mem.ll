; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; With the BWX extension (ev56 and later), byte and word memory accesses use
; ldbu/ldwu/stb/stw.  ldbu/ldwu zero-extend; a signed load is an extending load
; followed by a shift-based sign-extension.

; CHECK-LABEL: zext8:
; CHECK:       ldbu $0, 0($16)
; CHECK-NEXT:  ret
define i64 @zext8(ptr %p) {
  %v = load i8, ptr %p
  %r = zext i8 %v to i64
  ret i64 %r
}

; CHECK-LABEL: zext16:
; CHECK:       ldwu $0, 0($16)
; CHECK-NEXT:  ret
define i64 @zext16(ptr %p) {
  %v = load i16, ptr %p
  %r = zext i16 %v to i64
  ret i64 %r
}

; CHECK-LABEL: sext8:
; CHECK:       ldbu $0, 0($16)
; CHECK-NEXT:  sextb $0, $0
; CHECK-NEXT:  ret
define i64 @sext8(ptr %p) {
  %v = load i8, ptr %p
  %r = sext i8 %v to i64
  ret i64 %r
}

; CHECK-LABEL: store8:
; CHECK:       stb $17, 0($16)
; CHECK-NEXT:  ret
define void @store8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: store16:
; CHECK:       stw $17, 0($16)
; CHECK-NEXT:  ret
define void @store16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}
