; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -O2 < %s | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s --check-prefix=NOBWX

; A byte/word atomic store uses stb/stw with BWX; without BWX it must use the
; lock-based ldq_l/stq_c sequence, because a plain read-modify-write of the
; enclosing quadword is not atomic.

; BWX-LABEL: s8:
; BWX:   stb $17, 0($16)
; NOBWX-LABEL: s8:
; NOBWX: ldq_l
; NOBWX: stq_c
define void @s8(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p monotonic, align 1
  ret void
}

; BWX-LABEL: s16:
; BWX:   stw $17, 0($16)
; NOBWX-LABEL: s16:
; NOBWX: ldq_l
; NOBWX: stq_c
define void @s16(ptr %p, i16 %v) {
  store atomic i16 %v, ptr %p monotonic, align 2
  ret void
}
