; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 -O2 < %s | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s --check-prefix=NOBWX

; A naturally aligned byte or word atomic load reads the aligned quadword (which
; is atomic) and extracts the field: ldbu/ldwu with BWX, ldq_u + extbl/extwl
; without.

; BWX-LABEL: l8:
; BWX:   ldbu
; NOBWX-LABEL: l8:
; NOBWX: ldq_u
; NOBWX: extbl
define i64 @l8(ptr %p) {
  %v = load atomic i8, ptr %p monotonic, align 1
  %z = zext i8 %v to i64
  ret i64 %z
}

; BWX-LABEL: l16:
; BWX:   ldwu
; NOBWX-LABEL: l16:
; NOBWX: extwl
define i64 @l16(ptr %p) {
  %v = load atomic i16, ptr %p monotonic, align 2
  %z = zext i16 %v to i64
  ret i64 %z
}
