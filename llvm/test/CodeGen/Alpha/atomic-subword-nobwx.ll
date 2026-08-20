; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NOBWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev67 < %s | FileCheck %s --check-prefix=BWX

; sextb and sextw are BWX instructions.  Without BWX a byte or word is
; sign-extended by shifting it up to the top of the register and back down
; again, and none of the sub-word atomics may reach for the BWX form.

; The read-modify-write and compare-and-swap loops sign-extend their result the
; same way.  Without BWX that is an sll/sra pair, and it is checked for
; positively: emitting no sign extension at all would satisfy every NOT check
; below.  The pair sits inside the loop, between the load locked and the store
; conditional, because that is where the field it extends is read.
; NOBWX-LABEL: rmw_i8:
; NOBWX-NOT:   sextb
; NOBWX:       ldq_l
; NOBWX-NOT:   sextb
; NOBWX:       sll {{\$[0-9]+}}, 56, $0
; NOBWX-NEXT:  sra $0, 56, $0
; NOBWX-NOT:   sextb
; NOBWX:       stq_c
; NOBWX-NOT:   sextb
; BWX-LABEL:   rmw_i8:
; BWX:         sextb
define i8 @rmw_i8(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v monotonic
  ret i8 %r
}

; NOBWX-LABEL: cas_i16:
; NOBWX-NOT:   sextw
; NOBWX:       ldq_l
; NOBWX-NOT:   sextw
; NOBWX:       sll {{\$[0-9]+}}, 48, $0
; NOBWX-NEXT:  sra $0, 48, $0
; NOBWX-NOT:   sextw
; NOBWX:       stq_c
; NOBWX-NOT:   sextw
; BWX-LABEL:   cas_i16:
; BWX:         sextw
define i16 @cas_i16(ptr %p, i16 %c, i16 %n) {
  %r = cmpxchg ptr %p, i16 %c, i16 %n monotonic monotonic
  %v = extractvalue { i16, i1 } %r, 0
  ret i16 %v
}

; And so does the min/max expansion, which builds its own loop.
; The comparison inside the loop needs the extension as well as the result, so
; there are several.  These are order-independent because the blocks of the
; loop are not laid out in the order they run; what matters here is that the
; extensions exist at all.
; NOBWX-LABEL: umin_i8:
; NOBWX-NOT:   sextb
; NOBWX-DAG:   sll {{\$[0-9]+}}, 56, $0
; NOBWX-DAG:   sra $0, 56, $0
; NOBWX-DAG:   ldq_l
; NOBWX-DAG:   stq_c
; NOBWX-NOT:   sextb
define i8 @umin_i8(ptr %p, i8 %v) {
  %r = atomicrmw umin ptr %p, i8 %v monotonic
  ret i8 %r
}
