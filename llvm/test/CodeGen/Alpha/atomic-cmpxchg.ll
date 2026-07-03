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


; The success flag is the other half of the result: whether the value the loop
; read is the one that was expected.  The loop's own compare is inside the
; expansion, which happens after register allocation and so after the passes
; that would have shared it, so the flag is computed again on the way out.
; Either exit reaches it with the loaded value still in place, so one compare
; answers for both.
; CHECK-LABEL: cas_flag:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l $0, 0($16)
; CHECK-NEXT:  cmpeq $0, $17, [[E:\$[0-9]+]]
; CHECK-NEXT:  beq [[E]], [[OUT:\.LBB[0-9_]+]]
; CHECK:       stq_c [[N:\$[0-9]+]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK-NEXT: [[OUT]]:
; CHECK-NEXT:  cmpeq $0, $17, $0
; CHECK-NEXT:  ret
define i1 @cas_flag(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n monotonic monotonic
  %f = extractvalue { i64, i1 } %r, 1
  ret i1 %f
}
