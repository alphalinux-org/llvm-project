; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 -O0 < %s | FileCheck %s

; The three things the linker resolves to a fixed distance from the global
; pointer and which are not globals: a block address, a constant-pool entry, and
; the indirect branch that consumes the first of them.  All three are formed by
; the same ldah/lda pair against $29 -- the same one a gp-addressable global
; gets -- because none of them can be referred to from outside the object and
; so none of them needs a GOT entry.
;
; This is what LowerBlockAddress and LowerConstantPool build, and it has to be:
; an absolute address would put a text relocation in every shared library that
; used a switch or a floating-point literal.
;
; Both -O levels are checked because the constant-pool entry only appears when
; the constant is not one an lda can build, and -O0 reaches the load through a
; different path.

@tgt = global ptr null

; CHECK-LABEL: indirect:
; CHECK:       ldah $[[T:[0-9]+]], .Ltmp0($29)		!gprelhigh
; CHECK-NEXT:  lda ${{[0-9]+}}, .Ltmp0($[[T]])		!gprellow
; CHECK:       jmp $31, ($[[D:[0-9]+]]), 0
define i64 @indirect(i64 %n) {
entry:
  store ptr blockaddress(@indirect, %there), ptr @tgt
  %p = load ptr, ptr @tgt
  indirectbr ptr %p, [label %there, label %other]
there:
  ret i64 %n
other:
  ret i64 0
}

; The constant is loaded straight out of the pool: the gp-relative low half is
; the load's own displacement, so there is no separate lda.
; CHECK-LABEL: fp:
; CHECK:       ldah $[[C:[0-9]+]], .LCPI1_0($29)		!gprelhigh
; CHECK-NEXT:  ldt $f{{[0-9]+}}, .LCPI1_0($[[C]])		!gprellow
define double @fp(double %x) {
  %r = fadd double %x, 3.25
  ret double %r
}
