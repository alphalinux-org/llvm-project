; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=UNSAFE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-partial < %s | FileCheck %s --check-prefix=SAFE

; A misaligned store updates the quadword(s) it spans with a read-modify-write.
; By default that is a non-atomic ldq_u/stq_u pair; with -msafe-partial each
; spanned quadword is updated with a lock-based ldq_l/stq_c loop so a concurrent
; access to adjacent bytes is not lost.

; UNSAFE-LABEL: st:
; UNSAFE: ldq_u
; UNSAFE: stq_u
; UNSAFE-NOT: ldq_l

; Whether the field spans two quadwords depends on the address, so the second
; update is skipped at run time when the two aligned addresses are the same:
; without that, a field that lies in one quadword still takes a lock loop that
; writes back what it read.
;
; Two loops and no more: the ret directly after the second one's exit label is
; what says so.
; SAFE-LABEL: st:
; SAFE-NOT:   ldq_u
; SAFE:       cmpeq [[LO:\$[0-9]+]], [[HI:\$[0-9]+]], [[SAME:\$[0-9]+]]
; SAFE:      [[L1:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[A:\$[0-9]+]], 0([[LO]])
; SAFE-NEXT:  mskll [[A]], $16, [[A]]
; SAFE:       stq_c [[A]], 0([[LO]])
; SAFE-NEXT:  beq [[A]], [[L1]]
; SAFE:       bne [[SAME]], [[EXIT:\.LBB[0-9_]+]]
; SAFE:      [[L2:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[B:\$[0-9]+]], 0([[HI]])
; SAFE-NEXT:  msklh [[B]], $16, [[B]]
; SAFE:       stq_c [[B]], 0([[HI]])
; SAFE-NEXT:  beq [[B]], [[L2]]
; SAFE-NEXT: [[EXIT]]:
; SAFE-NEXT:  ret
define void @st(ptr %p, i32 %v) {
  store i32 %v, ptr %p, align 1
  ret void
}

; The width decides which mask pair the loops use, not how many there are: a
; misaligned access of any width spans one quadword or two depending on the
; address, so every width takes the same two loops with the same run-time skip
; between them.
; UNSAFE-LABEL: st16:
; UNSAFE: ldq_u
; UNSAFE-NOT: ldq_l
; SAFE-LABEL: st16:
; SAFE-NOT:   ldq_u
; SAFE:       cmpeq [[LO:\$[0-9]+]], [[HI:\$[0-9]+]], [[SAME:\$[0-9]+]]
; SAFE:      [[L1:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[A:\$[0-9]+]], 0([[LO]])
; SAFE-NEXT:  mskwl [[A]], $16, [[A]]
; SAFE:       stq_c [[A]], 0([[LO]])
; SAFE-NEXT:  beq [[A]], [[L1]]
; SAFE:       bne [[SAME]], [[EXIT:\.LBB[0-9_]+]]
; SAFE:      [[L2:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[B:\$[0-9]+]], 0([[HI]])
; SAFE-NEXT:  mskwh [[B]], $16, [[B]]
; SAFE:       stq_c [[B]], 0([[HI]])
; SAFE-NEXT:  beq [[B]], [[L2]]
; SAFE-NEXT: [[EXIT]]:
; SAFE-NEXT:  ret
define void @st16(ptr %p, i16 %v) {
  store i16 %v, ptr %p, align 1
  ret void
}

; UNSAFE-LABEL: st64:
; UNSAFE: ldq_u
; UNSAFE-NOT: ldq_l
; SAFE-LABEL: st64:
; SAFE-NOT:   ldq_u
; SAFE:       cmpeq [[LO:\$[0-9]+]], [[HI:\$[0-9]+]], [[SAME:\$[0-9]+]]
; SAFE:      [[L1:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[A:\$[0-9]+]], 0([[LO]])
; SAFE-NEXT:  mskql [[A]], $16, [[A]]
; SAFE:       stq_c [[A]], 0([[LO]])
; SAFE-NEXT:  beq [[A]], [[L1]]
; SAFE:       bne [[SAME]], [[EXIT:\.LBB[0-9_]+]]
; SAFE:      [[L2:\.LBB[0-9_]+]]:
; SAFE-NEXT:  ldq_l [[B:\$[0-9]+]], 0([[HI]])
; SAFE-NEXT:  mskqh [[B]], $16, [[B]]
; SAFE:       stq_c [[B]], 0([[HI]])
; SAFE-NEXT:  beq [[B]], [[L2]]
; SAFE-NEXT: [[EXIT]]:
; SAFE-NEXT:  ret
define void @st64(ptr %p, i64 %v) {
  store i64 %v, ptr %p, align 1
  ret void
}

; An aligned store is a plain stq under both settings: -msafe-partial is about
; the read-modify-write a misaligned store needs, and must not touch this.
; UNSAFE-LABEL: aligned:
; UNSAFE: stq $17, 0($16)
; UNSAFE-NOT: ldq_u
; SAFE-LABEL: aligned:
; SAFE: stq $17, 0($16)
; SAFE-NOT: ldq_l
define void @aligned(ptr %p, i64 %v) {
  store i64 %v, ptr %p, align 8
  ret void
}

; A volatile misaligned store keeps its memory operand through the custom
; inserter, so it is not merged with or hoisted past another access.  Both
; forms build a memory node for this reason; only the flags on it differ.
; UNSAFE-LABEL: vol:
; UNSAFE: ldq_u
; SAFE-LABEL: vol:
; SAFE: ldq_l
define void @vol(ptr %p, i32 %v) {
  store volatile i32 %v, ptr %p, align 1
  ret void
}
