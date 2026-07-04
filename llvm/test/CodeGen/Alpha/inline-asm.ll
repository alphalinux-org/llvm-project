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

; The "f" constraint selects a floating-point register for both f32 and f64.
; CHECK-LABEL: fp32:
; CHECK:       adds $f16, $f16, $f0
define float @fp32(float %x) {
  %r = call float asm "adds $1,$1,$0", "=f,f"(float %x)
  ret float %r
}

; CHECK-LABEL: fp64:
; CHECK:       addt $f16, $f16, $f0
define double @fp64(double %x) {
  %r = call double asm "addt $1,$1,$0", "=f,f"(double %x)
  ret double %r
}

; An integer bound to a floating-point register is 64 bits wide -- the FPCR,
; which mf_fpcr and mt_fpcr name a floating register for, is what this exists
; for -- so an i64 "f" operand takes the same register class an f64 does, and
; moves through memory rather than through an integer-to-float register move
; the hardware does not have without FIX.
; CHECK-LABEL: fp_i64_out:
; CHECK:       cpys $f31, $f31, $f0
; CHECK:       stt $f0, {{[0-9]+}}($30)
; CHECK:       ldq $0, {{[0-9]+}}($30)
define i64 @fp_i64_out() {
  %r = call i64 asm "cpys $$f31, $$f31, $0", "=f"()
  ret i64 %r
}

; CHECK-LABEL: fp_i64_in:
; CHECK:       stq $16, {{[0-9]+}}($30)
; CHECK:       ldt $f0, {{[0-9]+}}($30)
; CHECK:       cpys $f0, $f0, $f31
define void @fp_i64_in(i64 %v) {
  call void asm sideeffect "cpys $0, $0, $$f31", "f"(i64 %v)
  ret void
}
