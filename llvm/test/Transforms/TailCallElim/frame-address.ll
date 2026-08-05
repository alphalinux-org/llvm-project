; RUN: opt < %s -passes=tailcallelim -S | FileCheck %s

; A tail call tears the frame down before the callee runs, so a call that hands
; the callee a pointer naming this function's own frame must not be marked tail.
;
; The checks are anchored with {{^}} so that a positive check for
; `call void @callee' does not also match the `tail call void @callee' form.

declare ptr @llvm.frameaddress.p0(i32)
declare ptr @llvm.localaddress()
declare ptr @llvm.addressofreturnaddress.p0()
declare ptr @llvm.sponentry.p0()
declare ptr @llvm.stacksave.p0()
declare ptr @llvm.eh.dwarf.cfa(i32)
declare void @llvm.stackrestore.p0(ptr)
declare void @callee(ptr)

; CHECK-LABEL: @pass_frameaddress(
; CHECK: {{^}}  call void @callee
define void @pass_frameaddress() {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  call void @callee(ptr %fa)
  ret void
}

; llvm.localaddress hands out the frame pointer llvm.localrecover consumes, and
; Windows SEH and C++ EH pass it to an outlined funclet by value.
; CHECK-LABEL: @pass_localaddress(
; CHECK: {{^}}  call void @callee
define void @pass_localaddress() {
  %la = call ptr @llvm.localaddress()
  call void @callee(ptr %la)
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

; CHECK-LABEL: @pass_stacksave(
; CHECK: {{^}}  call void @callee
define void @pass_stacksave() {
  %ss = call ptr @llvm.stacksave.p0()
  call void @callee(ptr %ss)
  ret void
}

; llvm.eh.dwarf.cfa hands out the canonical frame address, which names this
; frame exactly as llvm.frameaddress does.
; CHECK-LABEL: @pass_dwarf_cfa(
; CHECK: {{^}}  call void @callee
define void @pass_dwarf_cfa() {
  %cfa = call ptr @llvm.eh.dwarf.cfa(i32 0)
  call void @callee(ptr %cfa)
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

; llvm.stackrestore consumes the token llvm.stacksave produced rather than
; letting the frame outlive the call, so it does not make the local stack
; escape and the call after it is still a tail call.
; CHECK-LABEL: @stackrestore_is_not_an_escape(
; CHECK: {{^}}  tail call void @callee
define void @stackrestore_is_not_an_escape(ptr %p) {
  %ss = call ptr @llvm.stacksave.p0()
  call void @llvm.stackrestore.p0(ptr %ss)
  call void @callee(ptr %p)
  ret void
}

; Only the llvm.stacksave token is exempt above.  An alloca handed to
; llvm.stackrestore is written through and still escapes.
; CHECK-LABEL: @stackrestore_of_an_alloca_is_an_escape(
; CHECK: {{^}}  call void @callee
define void @stackrestore_of_an_alloca_is_an_escape(ptr %p) {
  %a = alloca i8
  call void @llvm.stackrestore.p0(ptr %a)
  call void @callee(ptr %p)
  ret void
}
