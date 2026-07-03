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
