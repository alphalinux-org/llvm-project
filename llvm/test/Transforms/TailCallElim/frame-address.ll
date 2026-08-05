; RUN: opt < %s -passes=tailcallelim -S | FileCheck %s

; A tail call tears the frame down before the callee runs, so a call that hands
; the callee a pointer naming this function's own frame must not be marked tail.
;
; The checks are anchored with {{^}} on purpose.  An unanchored match for
; `call void @callee' also matches inside `tail call void @callee' and consumes
; that line, so a following negative check starts after it and never sees the
; `tail' -- the test would pass with every call below wrongly marked.

declare ptr @llvm.frameaddress.p0(i32)
declare ptr @llvm.addressofreturnaddress.p0()
declare ptr @llvm.sponentry.p0()
declare ptr @llvm.stacksave.p0()
declare void @callee(ptr)

; CHECK-LABEL: @pass_frameaddress(
; CHECK: {{^}}  call void @callee
define void @pass_frameaddress() {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  call void @callee(ptr %fa)
  ret void
}

; CHECK-LABEL: @pass_addressofreturnaddress(
; CHECK: {{^}}  call void @callee
define void @pass_addressofreturnaddress() {
  %ra = call ptr @llvm.addressofreturnaddress.p0()
  call void @callee(ptr %ra)
  ret void
}

; CHECK-LABEL: @pass_sponentry(
; CHECK: {{^}}  call void @callee
define void @pass_sponentry() {
  %sp = call ptr @llvm.sponentry.p0()
  call void @callee(ptr %sp)
  ret void
}

; llvm.stacksave is in the same list in the pass and had no test at all.
; CHECK-LABEL: @pass_stacksave(
; CHECK: {{^}}  call void @callee
define void @pass_stacksave() {
  %ss = call ptr @llvm.stacksave.p0()
  call void @callee(ptr %ss)
  ret void
}

; The pointer reaching the callee through a bitcast or a gep is still this
; frame's, so the call is still not a tail call.
; CHECK-LABEL: @pass_frameaddress_gep(
; CHECK: {{^}}  call void @callee
define void @pass_frameaddress_gep() {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  %p = getelementptr i8, ptr %fa, i64 8
  call void @callee(ptr %p)
  ret void
}

; A call that is handed nothing from this frame is still a tail call.
; CHECK-LABEL: @pass_nothing(
; CHECK: {{^}}  tail call void @callee
define void @pass_nothing(ptr %p) {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  call void @callee(ptr %p)
  ret void
}
