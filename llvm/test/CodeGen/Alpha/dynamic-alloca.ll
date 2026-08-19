; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s

; A variable-sized alloca needs a frame pointer: the prologue saves the caller's
; $15 and copies $30 into $15; fixed frame slots (here the saved $26) are then
; reached through $15 while the alloca subtracts from $30.  The epilogue
; restores $30 from $15, then the caller's $15, then deallocates the frame.

declare i64 @use(ptr)

; CHECK-LABEL: f:
; CHECK:      lda $30, -{{[0-9]+}}($30)
; CHECK:      stq $15, {{[0-9]+}}($30)
; CHECK:      bis $31, $30, $15
; CHECK:      subq $30, {{\$[0-9]+}}, {{\$[0-9]+}}
; CHECK:      bis $31, {{\$[0-9]+}}, $30
; CHECK:      jsr $26, ($27)
; CHECK:      bis $31, $15, $30
; CHECK:      ldq $15, {{[0-9]+}}($30)
; CHECK:      lda $30, {{[0-9]+}}($30)
; CHECK:      ret
define i64 @f(i64 %n) {
  %p = alloca i64, i64 %n
  %r = call i64 @use(ptr %p)
  ret i64 %r
}
