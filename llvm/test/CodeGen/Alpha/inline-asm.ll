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
