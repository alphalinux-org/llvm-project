; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel \
; RUN:   -stop-after=irtranslator -o - %s | FileCheck %s

; What the call lowering decides, before any of it reaches a machine
; instruction: which registers the arguments arrive in, where the ones past
; them come from, and where a result is handed back.  Six integer arguments go
; in $16-$21 and the seventh comes off the stack, which is the boundary the
; rule is about -- a lowering that placed them all in registers would name
; registers that hold nothing.

; CHECK-LABEL: name: args
; CHECK: fixedStack:
; CHECK-NEXT: - { id: 0, type: default, offset: 0, size: 8
; CHECK: %0:_(s64) = COPY $r16
; CHECK: %1:_(s64) = COPY $r17
; CHECK: %2:_(s64) = COPY $r18
; CHECK: %3:_(s64) = COPY $r19
; CHECK: %4:_(s64) = COPY $r20
; CHECK: %5:_(s64) = COPY $r21
; CHECK: %7:_(p0) = G_FRAME_INDEX %fixed-stack.0
; CHECK: %6:_(s64) = G_LOAD %7(p0) :: (load (s64) from %fixed-stack.0
; CHECK: $r0 = COPY
define i64 @args(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g) {
  %s = add i64 %a, %g
  ret i64 %s
}

declare i64 @callee(i64, i64)

; The outgoing side of the same rule, and the result coming back in $0.
; CHECK-LABEL: name: docall
; CHECK: ADJCALLSTACKDOWN
; CHECK-DAG: $r16 = COPY
; CHECK-DAG: $r17 = COPY
; CHECK: JSRd @callee
; CHECK: COPY $r0
; CHECK: ADJCALLSTACKUP
define i64 @docall(i64 %x) {
  %r = call i64 @callee(i64 %x, i64 7)
  ret i64 %r
}
