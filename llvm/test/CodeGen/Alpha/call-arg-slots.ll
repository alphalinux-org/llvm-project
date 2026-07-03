; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; The six argument slots are shared between the integer and floating registers:
; the Nth argument uses $16+N or $f16+N according to its type, so a mix of
; integer and floating arguments keeps a single running slot index rather than
; filling $16.. and $f16.. independently.

; CHECK-LABEL: mixed:
; a -> $16 (slot 0), b -> $f17 (slot 1), c -> $18 (slot 2), d -> $f19 (slot 3).
; CHECK-DAG:   stq $16,
; CHECK-DAG:   stq $18,
; CHECK-DAG:   addt $f17, $f19, {{\$f[0-9]+}}
; CHECK:       ret
define double @mixed(i64 %a, double %b, i64 %c, double %d) {
  %ac = sitofp i64 %a to double
  %cc = sitofp i64 %c to double
  %r0 = fadd double %b, %d
  %r1 = fadd double %r0, %ac
  %r2 = fadd double %r1, %cc
  ret double %r2
}

; A call passes the same way: the trailing int copies to $18, the float to $f19.
; CHECK-LABEL: caller:
; CHECK-DAG:   bis $31, $16, $18
; CHECK-DAG:   cpys $f17, $f17, $f19
; CHECK:       jsr $26, ($27)
define double @caller(i64 %x, double %y) {
  %r = call double @mixed(i64 %x, double %y, i64 %x, double %y)
  ret double %r
}
