; RUN: not llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s 2>&1 \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s -o /dev/null

; X_floating arithmetic survives -mno-fp-regs: _OtsAddX and its neighbours take
; a quadword pair in $16/$17 and $18/$19 and return one in $16/$17, so they need
; no floating-point register and are called the same way with the flag as
; without it (see f128-arith.ll).  The two conversions between X_floating and
; T/S_floating are the exception, and not by this backend's choice:
; _OtsConvertFloatTX reads its double from $f16, and _OtsConvertFloatXT returns
; one in $f0.  Those registers are part of the runtime's interface, so with the
; floating-point registers out of use there is no compatible way to make the
; call, and no answer to invent either: the flag's
; documented rule (`floating-point operands are passed in integer registers and
; floating-point results are passed in $0') describes the compiler's calling
; sequence, not a routine that already exists with a fixed one.
;
; Diagnose it.  Without this the copy to $f16 reaches the type legalizer, which
; cannot soften a copy to a physical register and aborts with `Do not know how
; to soften this operator's operand!' -- an internal error with no source
; location, for a program that is merely asking for something the flag rules
; out.

; CHECK: error: {{.*}} in function ext fp128 (double): converting between 'long double' and a shorter floating-point type calls a runtime routine that takes its argument in a floating-point register, so it cannot be done with -mno-fp-regs
define fp128 @ext(double %x) {
  %r = fpext double %x to fp128
  ret fp128 %r
}

; float goes the same way: it is widened to double first, and the widened value
; is what would be copied to $f16.
; CHECK: error: {{.*}} in function extf fp128 (float): converting between 'long double'
define fp128 @extf(float %x) {
  %r = fpext float %x to fp128
  ret fp128 %r
}

; CHECK: error: {{.*}} in function rnd double (fp128): converting between 'long double'
define double @rnd(fp128 %x) {
  %r = fptrunc fp128 %x to double
  ret double %r
}

; CHECK: error: {{.*}} in function rndf float (fp128): converting between 'long double'
define float @rndf(fp128 %x) {
  %r = fptrunc fp128 %x to float
  ret float %r
}

; The constrained forms are the same call, and are diagnosed the same way
; rather than reaching the legalizer.
; CHECK: error: {{.*}} in function sext fp128 (double): converting between 'long double'
define fp128 @sext(double %x) #0 {
  %r = call fp128 @llvm.experimental.constrained.fpext.f128.f64(
         double %x, metadata !"fpexcept.strict")
  ret fp128 %r
}

; CHECK: error: {{.*}} in function srnd double (fp128): converting between 'long double'
define double @srnd(fp128 %x) #0 {
  %r = call double @llvm.experimental.constrained.fptrunc.f64.f128(
         fp128 %x, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret double %r
}

attributes #0 = { strictfp }
