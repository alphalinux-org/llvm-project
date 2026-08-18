; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -mattr=+safe-bwa < %s \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -mattr=+safe-bwa < %s \
; RUN:   | FileCheck %s --check-prefix=BWX

; With -msafe-bwa and no BWX, a byte/word store does its read-modify-write with
; an ldq_l/stq_c loop so it is safe against concurrent accesses to the same
; quadword.  The aligned address and positioned field are computed once.

; CHECK-LABEL: storei8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       insbl $17, $16, [[I:\$[0-9]+]]
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK-NEXT:  mskbl {{\$[0-9]+}}, $16, {{\$[0-9]+}}
; CHECK-NEXT:  bis [[I]],
; CHECK-NEXT:  stq_c [[N:\$[0-9]+]], 0([[A]])
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret

; With BWX the hardware has a byte store, so -msafe-bwa has nothing to make
; safe and must not force the lock loop.
; BWX-LABEL: storei8:
; BWX:         stb $17, 0($16)
; BWX-NOT:     ldq_l
; BWX-NOT:     stq_c
define void @storei8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: storei16:
; CHECK:       inswl $17, $16,
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK:       stq_c [[N:\$[0-9]+]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define void @storei16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}
