; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; 32-bit atomic read-modify-writes use the ldl_l/stl_c primitives.

; CHECK-LABEL: add:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldl_l $0, 0($16)
; CHECK-NEXT:  addq $0, $17, [[N:\$[0-9]+]]
; CHECK-NEXT:  stl_c [[N]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i32 @add(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: xchg:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK:       ldl_l $0, 0($16)
; CHECK:       stl_c [[N:\$[0-9]+]], 0($16)
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i32 @xchg(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v monotonic
  ret i32 %r
}
