; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Sub-word compare-and-swap extracts the field, compares it against the
; expected value inside an ldq_l/stq_c loop and splices the new value back on a
; match.

; CHECK-LABEL: cas8:
; CHECK:       bic $16, 7, [[A:\$[0-9]+]]
; CHECK:       zapnot $17, 1, [[C:\$[0-9]+]]
; CHECK:       insbl $18, $16,
; CHECK:       ldq_l {{\$[0-9]+}}, 0([[A]])
; CHECK:       extbl {{\$[0-9]+}}, $16,
; CHECK:       cmpeq {{.*}}[[C]]
; CHECK:       mskbl {{\$[0-9]+}}, $16,
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
; CHECK:       zapnot $17, 3,
; CHECK:       inswl $18, $16,
; CHECK:       extwl {{\$[0-9]+}}, $16,
; CHECK:       mskwl {{\$[0-9]+}}, $16,
; CHECK:       stq_c
; CHECK:       beq
; CHECK:       ret
define i64 @cas16(ptr %p, i16 %c, i16 %n) {
  %r = cmpxchg ptr %p, i16 %c, i16 %n monotonic monotonic
  %ok = extractvalue { i16, i1 } %r, 1
  %z = zext i1 %ok to i64
  ret i64 %z
}
