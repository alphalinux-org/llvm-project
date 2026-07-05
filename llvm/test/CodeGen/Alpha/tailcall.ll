; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

declare i64 @callee(i64)
declare i64 @callee2(i64, i64)

; A direct tail call loads the callee's procedure value into $27 with our still
; valid gp, then jumps; the callee returns to our caller.  A jmp leaves $26
; untouched, so the function needs no frame and no $26 save.
; CHECK-LABEL: tail_direct:
; CHECK-NOT:  stq $26
; CHECK-NOT:  lda $30
; CHECK:      ldq $27, callee($29)
; CHECK-NOT:  jsr
; CHECK:      jmp $31, ($27), 0
define i64 @tail_direct(i64 %x) {
  %r = tail call i64 @callee(i64 %x)
  ret i64 %r
}

; An indirect tail call jumps through the function pointer, which is its own
; procedure value.
; CHECK-LABEL: tail_indirect:
; CHECK:      bis $31, $16, $27
; CHECK-NOT:  jsr
; CHECK:      jmp $31, ($27), 0
define i64 @tail_indirect(ptr %fp, i64 %x) {
  %r = tail call i64 %fp(i64 %x)
  ret i64 %r
}

; Passing two register arguments through a tail call is still a jump.
; CHECK-LABEL: tail_two:
; CHECK-NOT:  jsr
; CHECK:      jmp $31, ($27), 0
define i64 @tail_two(i64 %a, i64 %b) {
  %r = tail call i64 @callee2(i64 %b, i64 %a)
  ret i64 %r
}

; A call whose result is used before returning is not in tail position and keeps
; the jsr and gp reload.
; CHECK-LABEL: not_tail:
; CHECK:      jsr $26, ($27)
; CHECK-NEXT: ldgp $29, 0($26)
; CHECK:      ret
define i64 @not_tail(i64 %x) {
  %r = call i64 @callee(i64 %x)
  %s = add i64 %r, 1
  ret i64 %s
}

; A byval argument is a copy the caller made in its own frame and passes by
; address.  The epilogue runs before the jump, so tail-calling would leave the
; callee reading memory below the stack pointer.  Keep the jsr.
; CHECK-LABEL: tail_byval:
; CHECK:      jsr $26, ($27)
; CHECK-NOT:  jmp $31, ($27), 0
; CHECK:      ret
declare void @callee_byval(ptr byval(fp128) align 16)
define void @tail_byval(ptr byval(fp128) align 16 %x) {
  tail call void @callee_byval(ptr byval(fp128) align 16 %x)
  ret void
}

; The callee returns straight to our caller, so it has to return the way this
; function would have.  A fastcc caller and a C callee do not agree, and the
; call stays a call.
; CHECK-LABEL: caller_cc_mismatch:
; CHECK: jsr $26, ($27)
; CHECK: ret
define fastcc void @caller_cc_mismatch() {
  tail call void @c_callee()
  ret void
}

declare void @c_callee()
