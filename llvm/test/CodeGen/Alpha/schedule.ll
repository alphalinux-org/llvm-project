; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -O2 < %s | FileCheck %s --check-prefixes=CHECK,INORDER
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 -O2 < %s | FileCheck %s --check-prefixes=CHECK,INORDER
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -O2 < %s | FileCheck %s --check-prefixes=CHECK,EV6

; Each processor generation has its own scheduling model (21064/EV4,
; 21164/EV5, 21264/EV6), and the ordering is the whole point here, so these are
; ordered checks.  Order-independent ones would let the test pass with no
; scheduling model at all.
;
; The in-order generations issue a long-latency operation first and fill the
; shadow behind it with the independent work.  The 21264 is out of order and
; its multiply is much shorter, so it has no reason to hoist it.

; CHECK-LABEL: imul_hide:
; CHECK:        %bb.0:
; INORDER-NEXT: mulq
; INORDER-NEXT: addq $19, $20,
; EV6-NEXT:     addq $19, $20,
; EV6-NEXT:     mulq
; CHECK:        ret
define i64 @imul_hide(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e) {
  %m = mul i64 %a, %b
  %s = add i64 %m, %c
  %t = add i64 %d, %e
  %r = add i64 %s, %t
  ret i64 %r
}

; CHECK-LABEL: fdiv_hide:
; CHECK:        %bb.0:
; INORDER-NEXT: divt
; INORDER-NEXT: addt
; CHECK:        ret
define double @fdiv_hide(double %a, double %b, double %c, double %d) {
  %q = fdiv double %a, %b
  %s = fadd double %c, %d
  %r = fadd double %q, %s
  ret double %r
}
