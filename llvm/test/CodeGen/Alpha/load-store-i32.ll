; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; CHECK-LABEL: sextload:
; CHECK:       ldl $0, 0($16)
; CHECK-NEXT:  ret
define i64 @sextload(ptr %p) {
  %v = load i32, ptr %p
  %r = sext i32 %v to i64
  ret i64 %r
}

; CHECK-LABEL: zextload:
; CHECK:       ldl $0, 0($16)
; CHECK-NEXT:  zapnot $0, 15, $0
; CHECK-NEXT:  ret
define i64 @zextload(ptr %p) {
  %v = load i32, ptr %p
  %r = zext i32 %v to i64
  ret i64 %r
}

; CHECK-LABEL: store32:
; CHECK:       stl $17, 0($16)
; CHECK-NEXT:  ret
define void @store32(ptr %p, i32 %v) {
  store i32 %v, ptr %p
  ret void
}
