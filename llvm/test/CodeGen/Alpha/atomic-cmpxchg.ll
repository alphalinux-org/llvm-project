; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; cmpxchg expands to an ldq_l/stq_c loop that stores only when the loaded value
; matches the expected value.

; CHECK-LABEL: cas:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l $0, 0($16)
; CHECK-NEXT:  cmpeq $0, $17, [[EQ:\$[0-9]+]]
; A mismatch leaves the loop without storing, and lands past the store on the
; way out rather than back at the ldq_l.
; CHECK-NEXT:  beq [[EQ]], [[OUT:\.LBB[0-9_]+]]
; CHECK:       stq_c [[N:\$[0-9]+]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK-NEXT: [[OUT]]:
; CHECK-NEXT:  ret
define i64 @cas(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n monotonic monotonic
  %v = extractvalue { i64, i1 } %r, 0
  ret i64 %v
}


; The success flag is the other half of the result.  It is the comparison's own
; result, left in the return register, so the loop is the one above with no
; second compare and nothing else after it.
; CHECK-LABEL: cas_flag:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l $0, 0($16)
; CHECK-NEXT:  cmpeq $0, $17, $0
; CHECK-NEXT:  beq $0, [[OUT:\.LBB[0-9_]+]]
; CHECK:       stq_c [[N:\$[0-9]+]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK-NEXT: [[OUT]]:
; CHECK-NEXT:  ret
define i1 @cas_flag(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n monotonic monotonic
  %f = extractvalue { i64, i1 } %r, 1
  ret i1 %f
}
