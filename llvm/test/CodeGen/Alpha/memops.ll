; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; A small aligned memcpy/memset is expanded inline to quadword loads and stores
; instead of a library call; an unaligned or large copy keeps the call.

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

; 32 aligned bytes are four quadword copies, no call.
; CHECK-LABEL: cpy32:
; CHECK-NOT:  memcpy
; CHECK:      ldq
; CHECK:      stq
; CHECK-NOT:  memcpy
; CHECK:      ret
define void @cpy32(ptr align 8 %d, ptr align 8 %s) {
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %d, ptr align 8 %s, i64 32, i1 false)
  ret void
}

; A four-byte aligned copy is a single longword load/store.
; CHECK-LABEL: cpy4:
; CHECK:      ldl $0, 0($17)
; CHECK-NEXT: stl $0, 0($16)
; CHECK-NEXT: ret
define void @cpy4(ptr align 4 %d, ptr align 4 %s) {
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %d, ptr align 4 %s, i64 4, i1 false)
  ret void
}

; An aligned memset of zero stores the zero register directly.
; CHECK-LABEL: set32:
; CHECK-NOT:  memset
; CHECK:      stq $31, 0($16)
; CHECK:      ret
define void @set32(ptr align 8 %d) {
  call void @llvm.memset.p0.i64(ptr align 8 %d, i8 0, i64 32, i1 false)
  ret void
}

; An unaligned copy cannot use quadword accesses, so it keeps the library call.
; CHECK-LABEL: cpy_unaligned:
; CHECK:      memcpy
define void @cpy_unaligned(ptr %d, ptr %s) {
  call void @llvm.memcpy.p0.p0.i64(ptr %d, ptr %s, i64 16, i1 false)
  ret void
}
