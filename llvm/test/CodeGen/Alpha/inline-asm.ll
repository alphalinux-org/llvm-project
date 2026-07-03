; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The "r" constraint selects an integer register and the $N placeholders are
; substituted with the allocated registers.

; CHECK-LABEL: add:
; CHECK:       addq $16, $17, $0
; CHECK:       ret
define i64 @add(i64 %x, i64 %y) {
  %r = call i64 asm "addq $1,$2,$0", "=r,r,r"(i64 %x, i64 %y)
  ret i64 %r
}

; A clobber-only asm (memory barrier).
; CHECK-LABEL: barrier:
; CHECK:       mb
; CHECK:       ret
define void @barrier() {
  call void asm "mb", ""()
  ret void
}

; A memory-operand constraint is lowered to a base+displacement address.
; CHECK-LABEL: mem:
; CHECK:       ldq $0, 0($16)
define i64 @mem(ptr %p) {
  %r = call i64 asm "ldq $0, $1", "=r,*m"(ptr elementtype(i64) %p)
  ret i64 %r
}
