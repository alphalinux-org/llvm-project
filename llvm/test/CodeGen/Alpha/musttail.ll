; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s \
; RUN:   | FileCheck %s
; RUN: not --crash llc -mtriple=alpha-unknown-linux-gnu %S/Inputs/musttail-stack-args.ll \
; RUN:   2>&1 | FileCheck --check-prefix=ERR %s
; The GlobalISel call lowering refuses the same call, and refuses it by falling
; back rather than by diagnosing: the SelectionDAG path it falls back to makes
; the same decision and reports it, so there is one wording of the error.
; RUN: not --crash llc -mtriple=alpha-unknown-linux-gnu -global-isel \
; RUN:   %S/Inputs/musttail-stack-args.ll 2>&1 | FileCheck --check-prefix=ERR %s

; musttail is a guarantee the front end has already made to the user: the
; caller's frame is gone before the callee runs.  When the tail call cannot be
; made, emitting an ordinary call instead would trade a compile-time error for
; unbounded stack growth at run time, so the backend must refuse.
; ERR: failed to perform tail call elimination on a call site marked musttail

; A musttail call the backend can honour is lowered like any other tail call.
; CHECK-LABEL: good:
; CHECK-NOT:  stq $26
; CHECK:      jmp $31, ($27)
; CHECK-NOT:  ret
define dso_local i64 @target(i64 %x) {
  ret i64 %x
}
define i64 @good(i64 %x) {
  %r = musttail call i64 @target(i64 %x)
  ret i64 %r
}
