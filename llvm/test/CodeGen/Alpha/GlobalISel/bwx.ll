; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s \
; RUN:   | FileCheck %s --check-prefix=NOBWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+bwx -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=BWX

; The byte/word extension gives Alpha byte and word loads and stores.  Without
; it a sub-quadword access is an unaligned-quadword sequence, and a store is a
; read-modify-write.  Both arms live in AlphaInstructionSelector::selectLoadStore
; and nothing else in the GlobalISel test suite names +bwx, so without this
; test the BWX arm is selected only by the SelectionDAG tests.

; NOBWX-LABEL: load_i8:
; NOBWX:       ldq_u [[T:\$[0-9]+]], 0($16)
; NOBWX:       extbl [[T]], $16, {{\$[0-9]+}}
; BWX-LABEL:   load_i8:
; BWX:         ldbu $0, 0($16)
define i64 @load_i8(ptr %p) {
  %v = load i8, ptr %p
  %z = zext i8 %v to i64
  ret i64 %z
}

; NOBWX-LABEL: load_i16:
; NOBWX:       ldq_u [[T:\$[0-9]+]], 0($16)
; NOBWX:       extwl [[T]], $16, {{\$[0-9]+}}
; BWX-LABEL:   load_i16:
; BWX:         ldwu $0, 0($16)
define i64 @load_i16(ptr %p) {
  %v = load i16, ptr %p
  %z = zext i16 %v to i64
  ret i64 %z
}

; NOBWX-LABEL: store_i8:
; NOBWX:       ldq_u [[T:\$[0-9]+]], 0($16)
; NOBWX:       mskbl [[T]], $16, {{\$[0-9]+}}
; NOBWX:       stq_u {{\$[0-9]+}}, 0($16)
; BWX-LABEL:   store_i8:
; BWX:         stb $17, 0($16)
; BWX-NEXT:    ret
define void @store_i8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; NOBWX-LABEL: store_i16:
; NOBWX:       ldq_u [[T:\$[0-9]+]], 0($16)
; NOBWX:       inswl $17, $16, {{\$[0-9]+}}
; NOBWX:       stq_u {{\$[0-9]+}}, 0($16)
; BWX-LABEL:   store_i16:
; BWX:         stw $17, 0($16)
; BWX-NEXT:    ret
define void @store_i16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}
