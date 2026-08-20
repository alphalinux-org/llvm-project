; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -global-isel -global-isel-abort=1 < %s | FileCheck %s --check-prefixes=CHECK,FIX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev5 -global-isel -global-isel-abort=1 < %s | FileCheck %s --check-prefixes=CHECK,NOFIX

; The sequences are the ones AlphaInstructionSelector::selectFCmp builds; see
; the comment there for why each condition is spelled the way it is.
;
; Turning the 2.0/0.0 a compare leaves into a 0 or 1 is the one step that
; depends on the machine: with the FIX extension ftoit moves the bits and a
; shift right by 62 narrows them, and without it there is no integer/floating
; move at all, so the condition is branched on instead.  emitFCmpBit emits
; FCMPRES for the whole step and lets its custom inserter choose, so both
; machines are run here -- a test of only one would not notice the other
; falling back to the bitcast stack slot, which puts a frame on every one of
; these leaf functions.

; CHECK-LABEL: oeq:
; NOFIX-NOT:  stt
; CHECK:      cmpteq $f16, $f17, [[F:\$f[0-9]+]]
; FIX:        ftoit [[F]], $0
; FIX:        srl $0, 62, $0
; NOFIX:      fbne [[F]], .LBB
define i64 @oeq(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: une:
; CHECK:      cmpteq $f16, $f17, {{\$f[0-9]+}}
; FIX:        srl {{\$[0-9]+}}, 62, {{\$[0-9]+}}
; The inverted predicates are the ones selectFCmp builds by hand, and it
; spells out the move and the shift: without the FIX extension there is no
; integer/floating move, so the result goes through the bitcast stack slot and
; this leaf function gets a frame.
; NOFIX:      stt {{\$f[0-9]+}}, {{[0-9]+}}($30)
; NOFIX:      srl {{\$[0-9]+}}, 62, {{\$[0-9]+}}
; CHECK:      xor {{\$[0-9]+}}, 1, $0
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

; ord is the complement of uno, which cmptun tests directly: one compare and
; an inversion, not the a == a && b == b pair a target without cmptun needs.
; CHECK-LABEL: ord:
; CHECK:      cmptun $f16, $f17, {{\$f[0-9]+}}
; FIX:        srl {{\$[0-9]+}}, 62, {{\$[0-9]+}}
; NOFIX:      fbne {{\$f[0-9]+}}, .LBB
; CHECK:      xor {{\$[0-9]+}}, 1, $0
define i64 @ord(double %a, double %b) {
  %c = fcmp ord double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; uno is cmptun itself, so the truth value it leaves is the answer.
; CHECK-LABEL: uno:
; CHECK:      cmptun $f16, $f17, {{\$f[0-9]+}}
; FIX:        srl {{\$[0-9]+}}, 62, $0
; NOFIX:      fbne {{\$f[0-9]+}}, .LBB
; CHECK-NOT:  xor
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
; CHECK:      cmptun $f16, $f16, {{\$f[0-9]+}}
; CHECK-NOT:  xor
define i64 @isnan(double %a) {
  %c = fcmp uno double %a, %a
  %r = zext i1 %c to i64
  ret i64 %r
}

; oge is the mirror of ole: the operands swap and the same instruction does
; the work.
; CHECK-LABEL: oge:
; CHECK:      cmptle $f17, $f16, [[F:\$f[0-9]+]]
; FIX:        srl {{\$[0-9]+}}, 62, $0
; NOFIX:      fbne [[F]], .LBB
define i64 @oge(double %a, double %b) {
  %c = fcmp oge double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}
