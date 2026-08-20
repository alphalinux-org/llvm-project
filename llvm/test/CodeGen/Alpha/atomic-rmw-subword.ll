; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Sub-word atomic read-modify-writes do their update inside an ldq_l/stq_c loop:
; extract the field, apply the operation, splice it back into the quadword.

; CHECK-LABEL: add8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK-NEXT:  extbl {{\$[0-9]+}}, $16, [[F:\$[0-9]+]]
; The returned old value is sign-extended.  This runs without BWX, so that is
; a shift up to the top of the register and back down rather than sextb.  It
; sits inside the loop, next to the extract whose result it reads: the loop is
; built after register allocation, so nothing sinks it out afterwards.
; CHECK-NEXT:  sll [[F]], 56, $0
; CHECK-NEXT:  sra $0, 56, $0
; CHECK-NEXT:  addq [[F]], $17,
; CHECK:       mskbl {{\$[0-9]+}}, $16,
; CHECK:       insbl {{\$[0-9]+}}, $16,
; CHECK:       stq_c [[N:\$[0-9]+]], 0([[A]])
; CHECK-NEXT:  beq [[N]], [[LOOP]]
; CHECK:       ret
define i8 @add8(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v monotonic
  ret i8 %r
}

; An exchange writes its operand back whatever the load returned, so
; positioning it does not depend on the loop and the expansion does it once,
; before the loop rather than inside it.
; CHECK-LABEL: xchg16:
; CHECK:       inswl $17, $16, [[P:\$[0-9]+]]
; CHECK-NEXT:  [[LOOP:\.LBB[0-9_]+]]:
; CHECK-NEXT:  ldq_l
; CHECK-NEXT:  extwl {{\$[0-9]+}}, $16, [[F:\$[0-9]+]]
; CHECK-NEXT:  sll [[F]], 48, $0
; CHECK-NEXT:  sra $0, 48, $0
; CHECK-NEXT:  mskwl {{\$[0-9]+}}, $16, [[C:\$[0-9]+]]
; There is no insert here: the merge takes the field positioned before the loop.
; CHECK-NEXT:  bis [[P]], [[C]], [[N:\$[0-9]+]]
; CHECK-NEXT:  stq_c [[N]],
; CHECK-NEXT:  beq [[N]], [[LOOP]]
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
