; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The va_list is a { char *base; int offset; } pair, so va_copy must copy both
; fields; copying only the pointer would leave the destination's offset
; uninitialized.  __offset is an int, so it is a four-byte access -- writing a
; quadword would run past the field into the struct's tail padding.

; CHECK-LABEL: copy:
; CHECK-DAG: ldq $0, 0($17)
; CHECK-DAG: ldl $1, 8($17)
; CHECK-DAG: stq $0, 0($16)
; CHECK-DAG: stl $1, 8($16)
; CHECK: ret
define void @copy(ptr %d, ptr %s) {
  call void @llvm.va_copy.p0(ptr %d, ptr %s)
  ret void
}

declare void @llvm.va_copy.p0(ptr, ptr)
