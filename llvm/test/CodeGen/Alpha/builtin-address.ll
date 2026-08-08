; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; __builtin_return_address(0) captures the return address, which arrives in $26.
; CHECK-LABEL: ra:
; CHECK: bis $31, $26, $0
define i64 @ra() {
  %a = call ptr @llvm.returnaddress(i32 0)
  %b = ptrtoint ptr %a to i64
  ret i64 %b
}

; __builtin_frame_address(0) returns the frame pointer, $15.  Taking it is what
; makes the function set one up, so the prologue has to establish $15 from $30
; before it is read.
; CHECK-LABEL: fa:
; CHECK:      stq $15, {{[0-9]+}}($30)
; CHECK-NEXT: .cfi_offset $15
; CHECK-NEXT: bis $31, $30, $15
; CHECK:      bis $31, $15, $0
define i64 @fa() {
  %a = call ptr @llvm.frameaddress(i32 0)
  %b = ptrtoint ptr %a to i64
  ret i64 %b
}

; A frame does not record the one that called it, so no frame above this
; function's own can be named.  Answering zero needs no frame pointer.
; CHECK-LABEL: fa1:
; CHECK-NOT: $15
; CHECK: lda $0, 0($31)
define i64 @fa1() {
  %a = call ptr @llvm.frameaddress(i32 1)
  %b = ptrtoint ptr %a to i64
  ret i64 %b
}

declare ptr @llvm.returnaddress(i32)
declare ptr @llvm.frameaddress(i32)
