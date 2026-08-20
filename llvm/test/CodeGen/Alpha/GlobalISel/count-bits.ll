; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s \
; RUN:   | FileCheck %s --check-prefix=GENERIC
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+cix -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=CIX

; The count extension gives Alpha ctpop, ctlz and cttz.  Without it the
; legalizer lowers all three into the shift-and-mask sequence, which is why
; the register bank information's cases for them are reached only when the
; extension is present -- a subtarget-dependent path that a -mcpu=generic
; test can never enter.
;
; All three instructions answer 64 for a zero operand, which is what the
; non-poison forms require, so the poison and non-poison forms select the
; same instruction.

; GENERIC-LABEL: ctpop_:
; GENERIC-NOT:   ctpop $
; GENERIC:       mulq
; CIX-LABEL:     ctpop_:
; CIX:           ctpop $16, $0
; CIX-NEXT:      ret
define i64 @ctpop_(i64 %x) {
  %r = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %r
}

; GENERIC-LABEL: ctlz_:
; GENERIC-NOT:   ctlz $
; CIX-LABEL:     ctlz_:
; CIX:           ctlz $16, $0
; CIX-NEXT:      ret
define i64 @ctlz_(i64 %x) {
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 false)
  ret i64 %r
}

; GENERIC-LABEL: cttz_:
; GENERIC-NOT:   cttz $
; CIX-LABEL:     cttz_:
; CIX:           cttz $16, $0
; CIX-NEXT:      ret
define i64 @cttz_(i64 %x) {
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 false)
  ret i64 %r
}

; The zero-is-poison forms are lowered to the plain ones, so they must not
; grow a branch around the zero case.
; CIX-LABEL: ctlz_zero_poison:
; CIX:       ctlz $16, $0
; CIX-NEXT:  ret
define i64 @ctlz_zero_poison(i64 %x) {
  %r = call i64 @llvm.ctlz.i64(i64 %x, i1 true)
  ret i64 %r
}

; CIX-LABEL: cttz_zero_poison:
; CIX:       cttz $16, $0
; CIX-NEXT:  ret
define i64 @cttz_zero_poison(i64 %x) {
  %r = call i64 @llvm.cttz.i64(i64 %x, i1 true)
  ret i64 %r
}
