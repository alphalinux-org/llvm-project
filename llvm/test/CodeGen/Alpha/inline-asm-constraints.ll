; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; The GCC-compatible integer-constant constraints fold a suitable literal into
; the instruction, and a value that does not fit the constraint's range uses
; the register alternative of a combined constraint instead.

; CHECK-LABEL: useI:
; CHECK: and $16, 7, $0
define i64 @useI(i64 %x) {
  %r = call i64 asm "and $1, $2, $0", "=r,r,I"(i64 %x, i64 7)
  ret i64 %r
}

; CHECK-LABEL: useK:
; CHECK: lda $0, 100($16)
define i64 @useK(i64 %x) {
  %r = call i64 asm "lda $0, $2($1)", "=r,r,K"(i64 %x, i64 100)
  ret i64 %r
}

; A zero passed through the "r" operand modifier prints as the zero register.
; CHECK-LABEL: putJ:
; CHECK: stl $31,
define void @putJ(ptr %p) {
  call void asm sideeffect "stl ${0:r}, $1", "rJ,m"(i64 0, ptr %p)
  ret void
}

; A non-zero value with the same "rJ" constraint is placed in a register.
; CHECK-LABEL: putR:
; CHECK: stl $17,
define void @putR(ptr %p, i64 %x) {
  call void asm sideeffect "stl ${0:r}, $1", "rJ,m"(i64 %x, ptr %p)
  ret void
}

; A non-zero literal reaching the "r" modifier through an immediate constraint
; prints as itself.  gcc's %r special-cases only the zero, and an operate
; instruction takes an 8-bit literal where a register would go, so this is a
; well-formed operand rather than an "invalid operand in inline asm".
; CHECK-LABEL: addI_r:
; CHECK: addq $16, 7, $0
define i64 @addI_r(i64 %x) {
  %r = call i64 asm "addq $1, ${2:r}, $0", "=r,r,I"(i64 %x, i64 7)
  ret i64 %r
}

; A zap byte-mask: every byte of the value is kept whole or cleared.
; CHECK-LABEL: useM:
; CHECK: and $16, 65535, $0
define i64 @useM(i64 %x) {
  %r = call i64 asm "and $1, $2, $0", "=r,r,M"(i64 %x, i64 65535)
  ret i64 %r
}

; A value with a partial byte is not one, so the register alternative is used.
; CHECK-LABEL: notM:
; CHECK: lda [[R:\$[0-9]+]], 7($31)
; CHECK: and $16, [[R]], $0
define i64 @notM(i64 %x) {
  %r = call i64 asm "and $1, $2, $0", "=r,r,rM"(i64 %x, i64 7)
  ret i64 %r
}

; The remaining constant letters: an ldah constant, a complemented and a
; negated 8-bit value, a scale and a shift count.  Each prints as itself.
; CHECK-LABEL: useLNOPS:
; CHECK: ldah $16, 65536($31)
; CHECK: bic $16, -256, $16
; CHECK: subq $16, -255, $16
; CHECK: addq $16, 3, $16
; CHECK: sll $16, 63, $16
define void @useLNOPS(i64 %x) {
  call void asm sideeffect "ldah $0, $1($$31)", "r,L"(i64 %x, i64 65536)
  call void asm sideeffect "bic $0, $1, $0", "r,N"(i64 %x, i64 -256)
  call void asm sideeffect "subq $0, $1, $0", "r,O"(i64 %x, i64 -255)
  call void asm sideeffect "addq $0, $1, $0", "r,P"(i64 %x, i64 3)
  call void asm sideeffect "sll $0, $1, $0", "r,S"(i64 %x, i64 63)
  ret void
}

; The memory letter `Q' is GCC's "a memory operand that is not an AND-based
; reference to an unaligned location", which -- since nothing in this target
; forms such a reference -- is every memory operand, so it prints as the
; ordinary base-plus-displacement address.  Without a getInlineAsmMemConstraint
; that says so, the operand reaches SelectionDAGBuilder as
; ConstraintCode::Unknown and asserts.
; CHECK-LABEL: useQ:
; CHECK: ldq $0, 0($16)
define i64 @useQ(ptr %p) {
  %r = call i64 asm "ldq $0, $1", "=r,*Q"(ptr elementtype(i64) %p)
  ret i64 %r
}

; `R' is GCC's direct_call_operand: a symbol a bsr can reach, passed through by
; name rather than materialized into a register.
; CHECK-LABEL: useR:
; CHECK: bsr $26, ext
define void @useR() {
  call void asm sideeffect "bsr $$26, $0", "R"(ptr @ext)
  ret void
}

declare void @ext()
