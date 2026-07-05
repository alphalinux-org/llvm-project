; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Floating-point constants live in the constant pool and are addressed
; GP-relative: ldah !gprelhigh forms the high part of the offset from $gp, lda
; !gprellow the low part.  A double encoded as f32 is widened with cvtst.

; The !gprellow low part is folded into the load, so only the ldah !gprelhigh is
; needed to form the address.
; CHECK-LABEL: add1:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldah $0, .LCPI0_0($29){{.*}}!gprelhigh
; CHECK:       lds $f0, .LCPI0_0($0){{.*}}!gprellow
; CHECK:       cvtst $f0, $f0
; CHECK:       addt $f16, $f0, $f0
; CHECK:       ret
define double @add1(double %x) {
  %r = fadd double %x, 1.0
  ret double %r
}

; A constant that is not exactly representable as f32 is stored as f64 and
; loaded with ldt.
; CHECK-LABEL: addpi:
; CHECK:       ldah {{.*}}!gprelhigh
; CHECK:       ldt $f0, {{.*}}!gprellow
; CHECK:       addt $f16, $f0, $f0
; CHECK:       ret
define double @addpi(double %x) {
  %r = fadd double %x, 0x400921FB54442D18
  ret double %r
}

; A constant that would be a denormal as an f32 must stay an f64, even though it
; is exactly representable: lds maps an S_floating exponent of zero to zero, so
; a denormal read back that way is not the value that was written.  This one is
; __FLT_MIN__ / 2.
; CHECK-LABEL: adddenormal:
; CHECK:       ldah {{.*}}!gprelhigh
; CHECK:       ldt $f0, {{.*}}!gprellow
; CHECK-NOT:   lds
; CHECK:       addt $f16, $f0, $f0
; CHECK:       ret
define double @adddenormal(double %x) {
  %r = fadd double %x, 0x3800000000000000
  ret double %r
}
