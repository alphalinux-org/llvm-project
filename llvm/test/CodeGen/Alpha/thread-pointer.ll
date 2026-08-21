; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The thread pointer is the PALcode unique value rather than a register, so it
; is read and written by a PAL call: rduniq (0x9e) and wrunique (0x9f).  Both
; use $0 -- rduniq returns it there and wrunique takes it there -- which is why
; the write needs the copy and the read does not.

; CHECK-LABEL: get:
; CHECK:       call_pal 0x9e
; CHECK-NEXT:  ret
define ptr @get() {
  %p = call ptr @llvm.thread.pointer()
  ret ptr %p
}

; CHECK-LABEL: set:
; CHECK:       bis $31, $16, $0
; CHECK-NEXT:  call_pal 0x9f
; CHECK-NEXT:  ret
define void @set(ptr %p) {
  call void @llvm.alpha.set.thread.pointer(ptr %p)
  ret void
}

declare ptr @llvm.thread.pointer()
declare void @llvm.alpha.set.thread.pointer(ptr)
