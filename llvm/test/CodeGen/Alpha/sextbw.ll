; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev56 < %s \
; RUN:   | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=NOBWX

; BWX-LABEL: sext_i8:
; BWX:        sextb $16, $0
; NOBWX-LABEL: sext_i8:
; NOBWX:       sll $16, 56,
; NOBWX-NEXT:  sra
define i64 @sext_i8(i8 %x) {
  %r = sext i8 %x to i64
  ret i64 %r
}

; BWX-LABEL: sext_i16:
; BWX:        sextw $16, $0
; NOBWX-LABEL: sext_i16:
; NOBWX:       sll $16, 48,
; NOBWX-NEXT:  sra
define i64 @sext_i16(i16 %x) {
  %r = sext i16 %x to i64
  ret i64 %r
}

; The sub-word atomic loops sign-extend the field they return the same way, so
; the choice is made for them too -- and this is the only place the shift form
; would be reached with BWX available, since the loop is built after
; instruction selection rather than by a pattern.
; BWX-LABEL: atomic_sext_i8:
; BWX:        extbl {{\$[0-9]+}}, $16, [[F:\$[0-9]+]]
; BWX-NEXT:   sextb [[F]], $0
; NOBWX-LABEL: atomic_sext_i8:
; NOBWX:       extbl {{\$[0-9]+}}, $16, [[F:\$[0-9]+]]
; NOBWX-NEXT:  sll [[F]], 56, $0
; NOBWX-NEXT:  sra $0, 56, $0
define i8 @atomic_sext_i8(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v seq_cst
  ret i8 %r
}
