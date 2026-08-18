; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The va_list is a { base, offset } pair, so va_copy must copy both quadwords;
; copying only the pointer would leave the destination's offset uninitialized.

; CHECK-LABEL: copy:
; CHECK-DAG: ldq $0, 0($17)
; CHECK-DAG: ldq $1, 8($17)
; CHECK-DAG: stq $0, 0($16)
; CHECK-DAG: stq $1, 8($16)
; CHECK: ret
define void @copy(ptr %d, ptr %s) {
  call void @llvm.va_copy.p0(ptr %d, ptr %s)
  ret void
}

declare void @llvm.va_copy.p0(ptr, ptr)
