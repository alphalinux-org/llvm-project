; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs -global-isel \
; RUN:   -global-isel-abort=0 < %s | FileCheck %s

; -mno-fp-regs takes the floating-point file away, so a floating value has no
; register to be computed in and none to be passed in either.  The SelectionDAG
; path does not make the floating types legal under this option at all and
; lowers both of these to __adddf3.  GlobalISel has no soft-float lowering of
; its own: AlphaCallLowering refuses a signature that needs a floating register
; and AlphaLegalizerInfo refuses a floating operation, so the function goes to
; that path whole.  Both RUN lines check the same output, so the fallback has to
; produce what the SelectionDAG path produces -- which is the point of it.
;
; The second RUN line is -global-isel-abort=0 where the parity tests elsewhere
; use 1, because here the fallback is the behaviour being tested rather than a
; gap in it.  Without either refusal the selector reaches a floating instruction
; and register allocation fails with "no registers from class available to
; allocate", which invariant B3 reports first.

; This one is refused by the call lowering: the argument and the result both
; need a floating register.
; CHECK-LABEL: fadd_soft:
; CHECK:       jsr $26, ($27), __adddf3
; CHECK-NOT:   $f
define double @fadd_soft(double %a, double %b) {
  %r = fadd double %a, %b
  ret double %r
}

; This one has an all-integer signature, so the call lowering is content and it
; is the legalizer that refuses the fadd.
; CHECK-LABEL: fadd_soft_interior:
; CHECK:       jsr $26, ($27), __adddf3
; CHECK-NOT:   $f
define i64 @fadd_soft_interior(i64 %n, i64 %m) {
  %a = bitcast i64 %n to double
  %b = bitcast i64 %m to double
  %e = fadd double %a, %b
  %r = bitcast double %e to i64
  ret i64 %r
}
