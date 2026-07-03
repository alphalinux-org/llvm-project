; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev56 < %s \
; RUN:   | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=NOBWX

; BWX sign-extends a byte or word in one instruction; without it a shift pair.

; BWX-LABEL: sb:
; BWX:        sextb $16, $0
; NOBWX-LABEL: sb:
; NOBWX:       sll $16, 56,
; NOBWX-NEXT:  sra
define i64 @sb(i8 %x) {
  %r = sext i8 %x to i64
  ret i64 %r
}

; BWX-LABEL: sw:
; BWX:        sextw $16, $0
; NOBWX-LABEL: sw:
; NOBWX:       sll $16, 48,
; NOBWX-NEXT:  sra
define i64 @sw(i16 %x) {
  %r = sext i16 %x to i64
  ret i64 %r
}
