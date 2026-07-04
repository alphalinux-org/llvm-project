; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NONE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s --check-prefix=IEEE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee-with-inexact < %s \
; RUN:   | FileCheck %s --check-prefix=INEX

; Under -mieee the floating-point operate instructions take the software-
; completion qualifier: /su for arithmetic and compares, /sv for float-to-int;
; -mieee-with-inexact uses /sui and /svi, and also qualifies int-to-float.

define double @arith(double %a, double %b) {
; NONE: addt $f16, $f17
; IEEE: addt/su $f16, $f17
; INEX: addt/sui $f16, $f17
  %r = fadd double %a, %b
  ret double %r
}

define i64 @toint(double %a) {
; NONE: cvttq/c $f16
; IEEE: cvttq/svc $f16
; INEX: cvttq/svic $f16
  %r = fptosi double %a to i64
  ret i64 %r
}

define double @fromint(i64 %a) {
; NONE: cvtqt
; IEEE: cvtqt {{\$f[0-9]+}}
; INEX: cvtqt/sui
  %r = sitofp i64 %a to double
  ret double %r
}

define i64 @compare(double %a, double %b) {
; NONE: cmptlt $f16, $f17
; IEEE: cmptlt/su $f16, $f17
; INEX: cmptlt/su $f16, $f17
  %c = fcmp olt double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}
