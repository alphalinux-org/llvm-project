; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Floating-point comparisons leave 2.0 or 0.0 in an FP register; those bits are
; moved to an integer register and shifted right by 62 to give 0 or 1.

; CHECK-LABEL: oeq:
; CHECK:       cmpteq $f16, $f17, $f0
; CHECK:       srl $0, 62, $0
; CHECK:       ret
define i64 @oeq(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: olt:
; CHECK:       cmptlt $f16, $f17, $f0
; CHECK:       srl $0, 62, $0
; CHECK:       ret
define i64 @olt(double %a, double %b) {
  %c = fcmp olt double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; ogt swaps the operands.
; CHECK-LABEL: ogt:
; CHECK:       cmptlt $f17, $f16, $f0
; CHECK:       srl $0, 62, $0
; CHECK:       ret
define i64 @ogt(double %a, double %b) {
  %c = fcmp ogt double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ole:
; CHECK:       cmptle $f16, $f17, $f0
; CHECK:       ret
define i64 @ole(double %a, double %b) {
  %c = fcmp ole double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; f32 comparisons use the same T_floating instructions.
; CHECK-LABEL: olt_f32:
; CHECK:       cmptlt $f16, $f17, $f0
; CHECK:       ret
define i64 @olt_f32(float %a, float %b) {
  %c = fcmp olt float %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; There is no cmptne, so a NaN-agnostic inequality inverts cmpteq instead.  A
; non-NaN operand turns fcmp une into this, which is how MPFR's
; __gmpfr_ceil_log2 reaches it: it assembles a double in [1,2) out of integer
; bits and compares it against 1.0.
; CHECK-LABEL: ne:
; CHECK:       cmpteq $f16, $f17, $f0
; CHECK:       srl $0, 62, $0
; CHECK:       xor $0, 1, $0
; CHECK:       ret
define i64 @ne(double %a, double %b) {
  %c = fcmp nnan une double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: ne_f32:
; CHECK:       cmpteq $f16, $f17, $f0
; CHECK:       xor $0, 1, $0
; CHECK:       ret
define i64 @ne_f32(float %a, float %b) {
  %c = fcmp nnan une float %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; The same condition feeding a select inverts the fcmov instead.
; CHECK-LABEL: ne_select:
; CHECK:       cmpteq $f16, $f17, $f1
; CHECK:       fcmovne $f1, $f19, $f0
; CHECK:       ret
define double @ne_select(double %a, double %b, double %x, double %y) {
  %c = fcmp nnan une double %a, %b
  %s = select i1 %c, double %x, double %y
  ret double %s
}
