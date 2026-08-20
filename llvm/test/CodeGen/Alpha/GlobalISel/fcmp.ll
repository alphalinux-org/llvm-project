; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; The sequences are the ones AlphaInstructionSelector::selectFCmp builds; see
; the comment there for why each condition is spelled the way it is.

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

; ord is a == a && b == b, which is false as soon as either is a NaN.
; CHECK-LABEL: ord:
; CHECK:      cmpteq $f16, $f16, {{\$f[0-9]+}}
; CHECK:      srl {{\$[0-9]+}}, 62, [[A:\$[0-9]+]]
; CHECK:      cmpteq $f17, $f17, {{\$f[0-9]+}}
; CHECK:      srl {{\$[0-9]+}}, 62, [[B:\$[0-9]+]]
; CHECK:      and [[A]], [[B]], $0
define i64 @ord(double %a, double %b) {
  %c = fcmp ord double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: uno:
; CHECK:      cmpteq $f16, $f16, {{\$f[0-9]+}}
; CHECK:      cmpteq $f17, $f17, {{\$f[0-9]+}}
; CHECK:      and {{\$[0-9]+}}, {{\$[0-9]+}}, [[R:\$[0-9]+]]
; CHECK:      xor [[R]], 1, $0
define i64 @uno(double %a, double %b) {
  %c = fcmp uno double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; one is a < b || b < a: ordered, and false for a NaN either way round.
; CHECK-LABEL: one:
; CHECK:      cmptlt $f16, $f17, {{\$f[0-9]+}}
; CHECK:      cmptlt $f17, $f16, {{\$f[0-9]+}}
; CHECK:      bis {{\$[0-9]+}}, {{\$[0-9]+}}, $0
define i64 @one(double %a, double %b) {
  %c = fcmp one double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: ueq:
; CHECK:      cmptlt $f16, $f17, {{\$f[0-9]+}}
; CHECK:      cmptlt $f17, $f16, {{\$f[0-9]+}}
; CHECK:      bis {{\$[0-9]+}}, {{\$[0-9]+}}, [[R:\$[0-9]+]]
; CHECK:      xor [[R]], 1, $0
define i64 @ueq(double %a, double %b) {
  %c = fcmp ueq double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; isnan() from C: the front end emits fcmp uno against the value itself, which
; folds to one compare.
; CHECK-LABEL: isnan:
; CHECK:      cmpteq $f16, $f16, {{\$f[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @isnan(double %a) {
  %c = fcmp uno double %a, %a
  %r = zext i1 %c to i64
  ret i64 %r
}
