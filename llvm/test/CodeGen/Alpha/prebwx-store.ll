; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s | FileCheck %s

; Without BWX, a byte or word store reads the aligned quadword (ldq_u), clears
; the target field (mskbl/mskwl), splices in the positioned datum (insbl/inswl)
; and writes the quadword back (stq_u).
;
; The read, the mask and the insert are independent of each other, so the
; scheduler may emit them in any order; only the bis that combines them and the
; store that follows are ordered.  Checking a particular order here asserts a
; scheduling decision rather than the sequence, and pins whichever order the
; compiler happened to produce on the day.

; CHECK-LABEL: storei8:
; CHECK-DAG:   ldq_u {{\$[0-9]+}}, 0($16)
; CHECK-DAG:   mskbl {{\$[0-9]+}}, $16, {{\$[0-9]+}}
; CHECK-DAG:   insbl $17, $16, {{\$[0-9]+}}
; CHECK:       bis
; CHECK:       stq_u {{\$[0-9]+}}, 0($16)
; CHECK:       ret
define void @storei8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: storei16:
; CHECK-DAG:   ldq_u {{\$[0-9]+}}, 0($16)
; CHECK-DAG:   mskwl {{\$[0-9]+}}, $16, {{\$[0-9]+}}
; CHECK-DAG:   inswl $17, $16, {{\$[0-9]+}}
; CHECK:       bis
; CHECK:       stq_u {{\$[0-9]+}}, 0($16)
; CHECK:       ret
define void @storei16(ptr %p, i16 %v) {
  store i16 %v, ptr %p
  ret void
}
