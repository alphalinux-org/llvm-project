; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -filetype=obj < %s \
; RUN:   | llvm-readobj -r - | FileCheck --check-prefix=RELOC %s

; A direct tail call carries the same relocations a direct call does, so the
; linker can relax the GOT load and the jump into a single br: a lituse_jsr
; (addend 3) marking the jump as the literal's use, and no hint, which would
; pin the pair.  Only a callee that runs on our gp is tail-called, and such a
; callee is always dso-local, so the hint never appears on a jmp.
; RELOC:      R_ALPHA_LITERAL callee
; RELOC-NEXT: R_ALPHA_LITUSE - 0x3
; RELOC-NOT:  R_ALPHA_HINT callee

define dso_local i64 @callee(i64 %x) {
  ret i64 %x
}
define dso_local i64 @callee2(i64 %a, i64 %b) {
  %r = sub i64 %a, %b
  ret i64 %r
}

; A direct tail call loads the callee's procedure value into $27 with our still
; valid gp, then jumps; the callee returns to our caller.  A jmp leaves $26
; untouched, so the function needs no frame and no $26 save.
; CHECK-LABEL: tail_direct:
; CHECK-NOT:  stq $26
; CHECK-NOT:  lda $30
; The literal and the jump that uses it are written as a numbered pair.
; CHECK:      ldq $27, callee($29){{.*}}!literal![[N:[0-9]+]]
; CHECK-NOT:  jsr
; CHECK:      jmp $31, ($27){{[[:space:]]+}}!lituse_jsr![[N]]
define i64 @tail_direct(i64 %x) {
  %r = tail call i64 @callee(i64 %x)
  ret i64 %r
}

; An indirect callee's gp is unknowable, so the call stays a call: on return
; from a foreign gp our caller would find its own $29 gone, and the linker may
; already have deleted the reload that would have fixed it.
; CHECK-LABEL: tail_indirect:
; CHECK-NOT:  jmp $31, ($27)
; CHECK:      jsr $26, ($27)
; CHECK:      ret
define i64 @tail_indirect(ptr %fp, i64 %x) {
  %r = tail call i64 %fp(i64 %x)
  ret i64 %r
}

; A callee that is only declared here is defined in some other translation
; unit, which the linker may put in a gp region of its own.  Not a tail call.
; CHECK-LABEL: tail_external:
; CHECK-NOT:  jmp $31, ($27)
; CHECK:      ldq $27, external_callee($29){{.*}}!literal![[M:[0-9]+]]
; CHECK:      jsr $26, ($27){{[[:space:]]+}}!lituse_jsr![[M]]
; CHECK-NEXT: ldgp $29, 0($26)
; CHECK:      ret
declare dso_local i64 @external_callee(i64)
define i64 @tail_external(i64 %x) {
  %r = tail call i64 @external_callee(i64 %x)
  ret i64 %r
}

; The libm entry point behind a floating-point intrinsic is the same case, and
; the one that mattered: `return cos(x)' is the shape of every thin wrapper in
; a C library.  The callee is an ExternalSymbol with no GlobalValue to ask, and
; at run time it is the shared libm's, with libm's gp.
; CHECK-LABEL: tail_libcall:
; CHECK-NOT:  jmp $31, ($27)
; CHECK:      jsr $26, ($27), cos
; CHECK-NEXT: ldgp $29, 0($26)
; CHECK:      ret
define double @tail_libcall(double %x) {
  %r = tail call double @llvm.cos.f64(double %x)
  ret double %r
}

; A definition that can be interposed is replaced at run time by whichever DSO
; wins, so it is not ours to assume a gp for either.
; CHECK-LABEL: tail_interposable:
; CHECK-NOT:  jmp $31, ($27)
; CHECK:      jsr $26, ($27), interposable
; CHECK:      ret
define i64 @interposable(i64 %x) {
  ret i64 %x
}
define i64 @tail_interposable(i64 %x) {
  %r = tail call i64 @interposable(i64 %x)
  ret i64 %r
}

; Passing two register arguments through a tail call is still a jump.
; CHECK-LABEL: tail_two:
; CHECK-NOT:  jsr
; CHECK:      jmp $31, ($27){{[[:space:]]+}}!lituse_jsr
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
define dso_local void @callee_byval(ptr byval(fp128) align 16 %x) {
  ret void
}
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

define dso_local void @c_callee() {
  ret void
}
