; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A 4-byte atomic uses the longword load-locked/store-conditional pair, and its
; result is sign-extended (the Alpha canonical form for a longword in a register).

define i32 @rmw(ptr %p, i32 %v) {
; CHECK-LABEL: rmw:
; CHECK:      ldl_l {{\$[0-9]+}}, 0($16)
; CHECK:      stl_c {{\$[0-9]+}}, 0($16)
  %r = atomicrmw add ptr %p, i32 %v seq_cst
  ret i32 %r
}

define i32 @cas(ptr %p, i32 %e, i32 %n) {
; CHECK-LABEL: cas:
; The expected value is sign-extended to match the sign-extending ldl_l.
; CHECK:      addl $31, $17, [[E:\$[0-9]+]]
; CHECK:      ldl_l [[D:\$[0-9]+]], 0($16)
; CHECK:      cmpeq [[D]], [[E]],
; CHECK:      stl_c {{\$[0-9]+}}, 0($16)
  %r = cmpxchg ptr %p, i32 %e, i32 %n seq_cst seq_cst
  %v = extractvalue {i32, i1} %r, 0
  ret i32 %v
}
