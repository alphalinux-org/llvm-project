; RUN: llc -mtriple=alpha-unknown-linux-gnu -mtune=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mtune=ev5 < %s \
; RUN:   | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mtune=ev6 < %s \
; RUN:   | FileCheck %s --check-prefix=EV6
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -mtune=ev6 < %s \
; RUN:   | FileCheck %s --check-prefix=NOBWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev56 -mtune=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=BWX

; -mtune picks the scheduling model without changing which instructions may be
; emitted.  The in-order 21064 and 21164 issue the load early, to cover its
; latency behind the arithmetic; the out-of-order 21264 has no reason to and
; leaves it where the dependences put it.

; EV4-LABEL: sched:
; EV4:       mult $f16, $f17, $f0
; EV4-NEXT:  ldt $f1, 0($19)
; EV4-NEXT:  addt $f0, $f18, $f0

; EV6-LABEL: sched:
; EV6:       mult $f16, $f17, $f0
; EV6-NEXT:  addt $f0, $f18, $f0
; EV6-NEXT:  ldt $f1, 0($19)
define double @sched(double %a, double %b, double %c, ptr %p) {
  %m = fmul double %a, %b
  %l = load double, ptr %p
  %d = fadd double %m, %c
  %e = fadd double %d, %l
  %g = fmul double %e, %a
  ret double %g
}

; Which instructions exist is -mcpu's business alone.  Tuning for a processor
; that has BWX does not license a stb on one that does not, and tuning for one
; that lacks it does not withdraw the stb from one that has it.

; NOBWX-LABEL: byte:
; NOBWX:       mskbl
; NOBWX:       stq_u
; NOBWX-NOT:   stb

; BWX-LABEL: byte:
; BWX:       stb $17, 0($16)
; BWX-NOT:   stq_u
define void @byte(ptr %p, i8 %x) {
  store i8 %x, ptr %p
  ret void
}
