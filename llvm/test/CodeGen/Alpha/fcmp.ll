; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s --check-prefixes=CHECK,FIX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 < %s | FileCheck %s --check-prefixes=CHECK,NOFIX

; Floating-point comparisons leave 2.0 or 0.0 in an FP register, and a setcc
; wants 1 or 0 in an integer one.  How those bits cross depends on the machine.
; With the FIX extension ftoit moves them and a shift right by 62 narrows them.
; Without it Alpha has no integer/floating move at all: the bits would have to
; go through memory, which forces a frame onto a leaf function, so the compare
; is branched on instead -- the form gcc emits.  Both are checked, because a
; test of only one would not notice the other becoming a stack round trip.

; CHECK-LABEL: oeq:
; NOFIX-NOT:   stt
; CHECK:       cmpteq $f16, $f17, $f0
; FIX:         ftoit $f0, $0
; FIX:         srl $0, 62, $0
; NOFIX:       lda $0, 1($31)
; NOFIX:       fbne $f0, .LBB
; NOFIX:       lda $0, 0($31)
; CHECK:       ret
define i64 @oeq(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: olt:
; CHECK:       cmptlt $f16, $f17, $f0
; FIX:         srl $0, 62, $0
; NOFIX:       fbne $f0, .LBB
; CHECK:       ret
define i64 @olt(double %a, double %b) {
  %c = fcmp olt double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; ogt swaps the operands.
; CHECK-LABEL: ogt:
; CHECK:       cmptlt $f17, $f16, $f0
; FIX:         srl $0, 62, $0
; NOFIX:       fbne $f0, .LBB
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
; FIX:         srl $0, 62, $0
; NOFIX:       fbne $f0, .LBB
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

; The same condition feeding a select inverts the fcmov instead.  This one needs
; no integer register at all, on either machine.
; CHECK-LABEL: ne_select:
; CHECK-NOT:   ftoit
; CHECK-NOT:   fbne
; CHECK:       cmpteq $f16, $f17, $f1
; CHECK:       fcmovne $f1, $f19, $f0
; CHECK:       ret
define double @ne_select(double %a, double %b, double %x, double %y) {
  %c = fcmp nnan une double %a, %b
  %s = select i1 %c, double %x, double %y
  ret double %s
}

; The unordered predicate is an instruction of its own: cmptun writes 2.0 when
; either operand is a NaN.  Left to the generic expansion this is a
; self-comparison of each operand, and'ed and inverted -- eight instructions
; against three -- so the absence of a second compare is checked.
; CHECK-LABEL: uno:
; CHECK-NOT:   cmpteq
; CHECK:       cmptun $f16, $f17, $f0
; FIX:         srl $0, 62, $0
; NOFIX:       fbne $f0, .LBB
; CHECK:       ret
define i64 @uno(double %a, double %b) {
  %c = fcmp uno double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; CHECK-LABEL: uno_f32:
; CHECK-NOT:   cmpteq
; CHECK:       cmptun $f16, $f17, $f0
; CHECK:       ret
define i64 @uno_f32(float %a, float %b) {
  %c = fcmp uno float %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; Ordered is the complement of the same instruction, one xor more -- not a
; different comparison.
; CHECK-LABEL: ord:
; CHECK-NOT:   cmpteq
; CHECK:       cmptun $f16, $f17, $f0
; FIX:         srl $0, 62, $0
; NOFIX:       fbne $f0, .LBB
; CHECK:       xor $0, 1, $0
; CHECK:       ret
define i64 @ord(double %a, double %b) {
  %c = fcmp ord double %a, %b
  %z = zext i1 %c to i64
  ret i64 %z
}

; Feeding a select, cmptun's 2.0/0.0 result goes straight into fcmovne, with no
; trip through an integer register.
; CHECK-LABEL: uno_select:
; CHECK-NOT:   cmpteq
; CHECK:       cmptun $f16, $f17, $f1
; CHECK:       fcmovne $f1, $f18, $f0
; CHECK:       ret
define double @uno_select(double %a, double %b, double %x, double %y) {
  %c = fcmp uno double %a, %b
  %s = select i1 %c, double %x, double %y
  ret double %s
}
