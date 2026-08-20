; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Alpha has no floating-point read-modify-write, so these go the same way as
; the integer min/max forms: the atomic expander bitcasts the value to an
; integer of the same width and open-codes a compare-and-swap loop.  The
; arithmetic stays in the floating-point registers and the loop moves the
; result across with itoft/ftoit.

; CHECK-LABEL: fadd_f64:
; CHECK:       addt
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK:       ret
define double @fadd_f64(ptr %p, double %v) {
  %r = atomicrmw fadd ptr %p, double %v monotonic
  ret double %r
}

; CHECK-LABEL: fsub_f32:
; CHECK:       subs
; CHECK:       ldl_l
; CHECK:       stl_c
define float @fsub_f32(ptr %p, float %v) {
  %r = atomicrmw fsub ptr %p, float %v monotonic
  ret float %r
}

; CHECK-LABEL: fmax_f64:
; CHECK:       ldq_l
; CHECK:       stq_c
define double @fmax_f64(ptr %p, double %v) {
  %r = atomicrmw fmax ptr %p, double %v monotonic
  ret double %r
}

; The wrapping forms have no instruction either and take the same path.
; CHECK-LABEL: uinc_wrap:
; CHECK:       ldq_l
; CHECK:       stq_c
define i64 @uinc_wrap(ptr %p, i64 %v) {
  %r = atomicrmw uinc_wrap ptr %p, i64 %v monotonic
  ret i64 %r
}
