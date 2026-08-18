; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -mattr=+safe-bwa < %s \
; RUN:   | FileCheck %s

; With -msafe-bwa and no BWX, a byte/word store does its read-modify-write with
; an ldq_l/stq_c loop so it is safe against concurrent accesses to the same
; quadword.  The aligned address and positioned field are computed once.

; CHECK-LABEL: storei8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       insbl $17, $16, [[I:\$[0-9]+]]
; CHECK:       ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK:       mskbl {{\$[0-9]+}}, $16, {{\$[0-9]+}}
; CHECK:       bis [[I]],
; CHECK:       stq_c {{\$[0-9]+}}, 0([[A]])
; CHECK:       beq
; CHECK:       ret
define void @storei8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: storei16:
; CHECK:       inswl $17, $16,
; CHECK:       ldq_l
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK:       stq_c
; CHECK:       beq
; CHECK:       ret
define void @storei16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}
