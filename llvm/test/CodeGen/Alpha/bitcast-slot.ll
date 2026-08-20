; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Moving the bits of a value between an integer and a floating register bounces
; them through the stack.  Each bounce is a store followed immediately by its
; own load, so no two are ever live at once and one slot serves them all: the
; frame is 16 bytes whatever the number of moves, not eight more per move.

; CHECK-LABEL: four_moves:
; CHECK:      lda $30, -16($30)
; CHECK-NOT:  lda $30, -{{[0-9]+}}($30)
; CHECK:      lda $30, 16($30)
; CHECK:      ret
define double @four_moves(i64 %a, i64 %b) {
  %x = bitcast i64 %a to double
  %y = bitcast i64 %b to double
  %s = fadd double %x, %y
  %i = bitcast double %s to i64
  %j = add i64 %i, 1
  %r = bitcast i64 %j to double
  ret double %r
}
