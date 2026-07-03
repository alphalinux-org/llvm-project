; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Sub-word atomic read-modify-writes do their update inside an ldq_l/stq_c loop:
; extract the field, apply the operation, splice it back into the quadword.

; CHECK-LABEL: add8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK:       extbl {{\$[0-9]+}}, $16, $0
; CHECK:       addq $0, $17,
; CHECK:       mskbl {{\$[0-9]+}}, $16,
; CHECK:       insbl {{\$[0-9]+}}, $16,
; CHECK:       stq_c [[N:\$[0-9]+]], 0([[A]])
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i8 @add8(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v monotonic
  ret i8 %r
}

; The xchg operation is loop-invariant, so the position/extract steps are
; hoisted out of the loop.
; CHECK-LABEL: xchg16:
; CHECK-DAG:   inswl {{\$[0-9]+}}, $16,
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK:       stq_c [[N:\$[0-9]+]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       extwl {{\$[0-9]+}}, $16, $0
; CHECK:       ret
define i16 @xchg16(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v monotonic
  ret i16 %r
}

; The other ALU operations take the same splice, and a word-width one exercises
; the msk/ins pair at the other size: the operation lands between the extract
; and the insert rather than being applied to the whole quadword.
; CHECK-LABEL: and8:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK:       extbl
; CHECK:       and
; CHECK:       mskbl
; CHECK:       insbl
; CHECK:       stq_c [[N:\$[0-9]+]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
define i8 @and8(ptr %p, i8 %v) {
  %r = atomicrmw and ptr %p, i8 %v monotonic
  ret i8 %r
}

; CHECK-LABEL: or16:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK:       extwl
; CHECK:       bis
; CHECK:       mskwl
; CHECK:       inswl
; CHECK:       stq_c [[N:\$[0-9]+]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
define i16 @or16(ptr %p, i16 %v) {
  %r = atomicrmw or ptr %p, i16 %v monotonic
  ret i16 %r
}

; CHECK-LABEL: xor8:
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK:       extbl
; CHECK:       xor
; CHECK:       mskbl
; CHECK:       insbl
; CHECK:       stq_c [[N:\$[0-9]+]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
define i8 @xor8(ptr %p, i8 %v) {
  %r = atomicrmw xor ptr %p, i8 %v monotonic
  ret i8 %r
}
