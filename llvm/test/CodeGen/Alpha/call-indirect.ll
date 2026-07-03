; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; An indirect call uses the callee address already held in a register as the
; procedure value ($27); no GOT load is needed.

; The callee address arrives in $16 and has to reach $27, the procedure value
; the call jumps through, before the arguments are shuffled down.  Checking
; only for the jsr and the gp reload said nothing about that: both appear in
; every call.
; CHECK-LABEL: apply:
; CHECK:       bis $31, $16, $27
; CHECK:       jsr $26, ($27)
; CHECK:       ldgp $29, 0($26)
; CHECK:       ret
define i64 @apply(ptr %fp, i64 %x, i64 %y) {
  %r = call i64 %fp(i64 %x, i64 %y)
  ret i64 %r
}
