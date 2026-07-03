; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The first six integer arguments are passed in $16-$21; further arguments are
; passed on the stack in 8-byte slots in the caller's outgoing argument area.

; The seventh argument arrives at the bottom of the incoming area, 0($30).
; CHECK-LABEL: recv7:
; CHECK:       ldq $0, 0($30)
; CHECK-NEXT:  ret
define i64 @recv7(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g) {
  ret i64 %g
}

; The caller stores the seventh argument into the outgoing area before the call.
; The seventh argument is the one that goes on the stack, and it is the
; constant 42.  Matching any register stored to 0($30) does not say which
; argument ended up there -- the six that go in registers are shuffled around
; the same store.
; CHECK-LABEL: call7:
; CHECK:       lda $[[C:[0-9]+]], 42($31)
; CHECK-NEXT:  stq $[[C]], 0($30)
; CHECK:       jsr $26, ($27)
; CHECK:       ret
define i64 @call7(i64 %a) {
  %r = call i64 @recv7(i64 %a, i64 %a, i64 %a, i64 %a, i64 %a, i64 %a, i64 42)
  ret i64 %r
}
