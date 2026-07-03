; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; cmpxchg expands to an ldq_l/stq_c loop that stores only when the loaded value
; matches the expected value.

; CHECK-LABEL: cas:
; CHECK:       ldq_l $0, 0($16)
; CHECK:       cmpeq $0, $17, [[EQ:\$[0-9]+]]
; CHECK:       beq [[EQ]],
; CHECK:       stq_c {{\$[0-9]+}}, 0($16)
; CHECK:       beq
; CHECK:       ret
define i64 @cas(ptr %p, i64 %c, i64 %n) {
  %r = cmpxchg ptr %p, i64 %c, i64 %n monotonic monotonic
  %v = extractvalue { i64, i1 } %r, 0
  ret i64 %v
}
