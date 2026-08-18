; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; There is no instruction that puts a floating value in a register, so a
; floating constant is a constant pool entry loaded relative to the GP.  The
; width picks the load: a single is held in memory in S_floating format and
; lds converts it to the T_floating form the register holds, while a double is
; already in the register's format and ldt is a bit copy.  Selecting ldt for a
; single would load the four bytes after it as well.

; CHECK-LABEL: float_constant:
; CHECK:       ldgp $29, 0($27)
; CHECK:       ldah [[H:\$[0-9]+]], .LCPI{{[0-9_]+}}($29)
; CHECK:       lds $f0, .LCPI{{[0-9_]+}}([[H]])
define float @float_constant() {
  ret float 3.25
}

; CHECK-LABEL: double_constant:
; CHECK:       ldah [[H:\$[0-9]+]], .LCPI{{[0-9_]+}}($29)
; CHECK:       ldt $f0, .LCPI{{[0-9_]+}}([[H]])
define double @double_constant() {
  ret double 3.25
}

; Zero is the exception: $f31 reads as zero, so it needs no pool entry.
; CHECK-LABEL: float_zero:
; CHECK-NOT:   .LCPI
; CHECK:       cpys $f31, $f31, $f0
define float @float_zero() {
  ret float 0.0
}

; An undefined floating value is given a floating register and nothing else;
; it must not be materialised through the integer bank.
; CHECK-LABEL: undef_fp:
; CHECK-NOT:   ($30)
; CHECK:       addt {{\$f[0-9]+}}, $f16, $f0
define double @undef_fp(double %x) {
  %r = fadd double undef, %x
  ret double %r
}
