; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A floating compare leaves 2.0 or 0.0 in a floating register, so the answer is
; those bits moved into an integer register and shifted right by 62.  There are
; only the equal, less-than and less-or-equal instructions: the greater forms
; swap the operands and inequality is equality inverted.

; CHECK-LABEL: oeq:
; CHECK:      cmpteq $f16, $f17, [[F:\$f[0-9]+]]
; CHECK:      srl {{\$[0-9]+}}, 62, $0
define i64 @oeq(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: une:
; CHECK:      cmpteq $f16, $f17, {{\$f[0-9]+}}
; CHECK:      srl {{\$[0-9]+}}, 62, [[S:\$[0-9]+]]
; CHECK:      xor [[S]], 1, $0
define i64 @une(float %a, float %b) {
  %c = fcmp une float %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ogt:
; CHECK:      cmptlt $f17, $f16, {{\$f[0-9]+}}
define i64 @ogt(double %a, double %b) {
  %c = fcmp ogt double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ole:
; CHECK:      cmptle $f16, $f17, {{\$f[0-9]+}}
define i64 @ole(double %a, double %b) {
  %c = fcmp ole double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; An unordered relation is the negation of the opposite ordered one, and that
; holds for a NaN too: every ordered compare against one is false, and inverting
; gives the true these want.

; CHECK-LABEL: uge:
; CHECK:      cmptlt $f16, $f17, {{\$f[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @uge(double %a, double %b) {
  %c = fcmp uge double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ugt:
; CHECK:      cmptle $f16, $f17, {{\$f[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @ugt(double %a, double %b) {
  %c = fcmp ugt double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; The less-than forms swap the operands as well as inverting.
; CHECK-LABEL: ult:
; CHECK:      cmptle $f17, $f16, {{\$f[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @ult(double %a, double %b) {
  %c = fcmp ult double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ule:
; CHECK:      cmptlt $f17, $f16, {{\$f[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @ule(double %a, double %b) {
  %c = fcmp ule double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}
