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
