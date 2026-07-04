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
; NOBWX: ldq_u
; NOBWX: extwl
define i64 @l16(ptr %p) {
  %v = load atomic i16, ptr %p monotonic, align 2
  %z = zext i16 %v to i64
  ret i64 %z
}

; getExtendForAtomicOps says a narrow atomic result is sign-extended, so a
; sign-extending load needs a pattern of its own.  Without BWX that extension is
; a shift up to the top of the register and back down, which an EV4 or EV5 does
; not trap on; sextb and sextw are BWX instructions and must not be reached for
; without it.  The shift pair is checked positively -- emitting no extension at
; all would satisfy a NOT check on its own.

; BWX-LABEL: sl8:
; BWX:   ldbu
; BWX:   sextb
; NOBWX-LABEL: sl8:
; NOBWX-NOT: sextb
; NOBWX: extbl
; NOBWX: sll {{\$[0-9]+}}, 56, [[U:\$[0-9]+]]
; NOBWX-NEXT: sra [[U]], 56,
define i8 @sl8(ptr %p) {
  %v = load atomic i8, ptr %p monotonic, align 1
  ret i8 %v
}

; BWX-LABEL: sl16:
; BWX:   ldwu
; BWX:   sextw
; NOBWX-LABEL: sl16:
; NOBWX-NOT: sextw
; NOBWX: extwl
; NOBWX: sll {{\$[0-9]+}}, 48, [[U:\$[0-9]+]]
; NOBWX-NEXT: sra [[U]], 48,
define i16 @sl16(ptr %p) {
  %v = load atomic i16, ptr %p monotonic, align 2
  ret i16 %v
}
