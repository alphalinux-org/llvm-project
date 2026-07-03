; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Atomic read-modify-writes expand to an ldq_l/stq_c retry loop.  The value read
; before the operation is the result; stq_c reports success in its source
; register, and beq retries on failure.

; CHECK-LABEL: add:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l $0, 0($16)
; CHECK-NEXT:  addq $0, $17, [[N:\$[0-9]+]]
; CHECK-NEXT:  stq_c [[N]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i64 @add(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: xchg:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK:       ldq_l $0, 0($16)
; CHECK:       stq_c [[N:\$[0-9]+]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i64 @xchg(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: and:
; CHECK:       ldq_l $0, 0($16)
; CHECK-NEXT:  and $0, $17,
; CHECK:       ret
define i64 @and(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v monotonic
  ret i64 %r
}
