; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Sub-word compare-and-swap extracts the field, compares it against the
; expected value inside an ldq_l/stq_c loop and splices the new value back on a
; match.

; The comparand is sign-extended to the canonical register form first: the
; success flag compares it against the extracted field, which is sign-extended
; the same way, and an any-extended negative comparand would never match.
; CHECK-LABEL: cas8:
; CHECK:       sll $17, 56, [[SX:\$[0-9]+]]
; CHECK-NEXT:  sra [[SX]], 56, [[SX]]
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       zapnot [[SX]], 1, [[C:\$[0-9]+]]
; CHECK:       ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK:       extbl {{\$[0-9]+}}, $16,
; CHECK:       cmpeq {{.*}}[[C]]
; The new value is only positioned once the comparison has matched, so it costs
; nothing on the path that finds the field already changed.
; CHECK:       mskbl {{\$[0-9]+}}, $16,
; CHECK-NEXT:  insbl $18, $16,
; CHECK:       stq_c {{\$[0-9]+}}, 0([[A]])
; CHECK:       beq
; CHECK:       ret
define i64 @cas8(ptr %p, i8 %c, i8 %n) {
  %r = cmpxchg ptr %p, i8 %c, i8 %n monotonic monotonic
  %ok = extractvalue { i8, i1 } %r, 1
  %z = zext i1 %ok to i64
  ret i64 %z
}

; CHECK-LABEL: cas16:
; CHECK:       sll $17, 48, [[SX16:\$[0-9]+]]
; CHECK-NEXT:  sra [[SX16]], 48, [[SX16]]
; CHECK:       zapnot [[SX16]], 3,
; CHECK:       extwl {{\$[0-9]+}}, $16,
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK-NEXT:  inswl $18, $16,
; CHECK:       stq_c
; CHECK:       beq
; CHECK:       ret
define i64 @cas16(ptr %p, i16 %c, i16 %n) {
  %r = cmpxchg ptr %p, i16 %c, i16 %n monotonic monotonic
  %ok = extractvalue { i16, i1 } %r, 1
  %z = zext i1 %ok to i64
  ret i64 %z
}
