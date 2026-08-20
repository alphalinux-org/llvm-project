; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

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

; A non-zero displacement is the case the lowering exists for: with only the
; zero-offset form checked, an implementation that dropped it would pass.
; CHECK-LABEL: mem_disp:
; CHECK:       ldq $0, 24($16)
define i64 @mem_disp(ptr %p) {
  %q = getelementptr i64, ptr %p, i64 3
  %r = call i64 asm "ldq $0, $1", "=r,*m"(ptr elementtype(i64) %q)
  ret i64 %r
}
