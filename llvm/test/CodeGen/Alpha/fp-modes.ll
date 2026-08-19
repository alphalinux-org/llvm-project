; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NONE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s --check-prefix=SU
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee-with-inexact < %s | FileCheck %s --check-prefix=SUI
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fptrap-u < %s | FileCheck %s --check-prefix=U
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fpround-dynamic < %s | FileCheck %s --check-prefix=DYN
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee,+fpround-dynamic < %s | FileCheck %s --check-prefix=SUD

; -mieee and -mfp-rounding-mode decide which qualifier the instructions a
; compilation generates carry.  That is a codegen question, so it is asked here
; rather than of the assembler: what an assembly file wrote is what it meant,
; and MC/Alpha/fp-qualifiers.s covers that side.

define double @arith(double %a, double %b) {
  %r = fadd double %a, %b
  ret double %r
}
; NONE-LABEL: arith:
; NONE:       addt $f16, $f17, $f0
; SU-LABEL:   arith:
; SU:         addt/su $f16, $f17, $f0
; SUI-LABEL:  arith:
; SUI:        addt/sui $f16, $f17, $f0
; U-LABEL:    arith:
; U:          addt/u $f16, $f17, $f0
; DYN-LABEL:  arith:
; DYN:        addt/d $f16, $f17, $f0
; SUD-LABEL:  arith:
; SUD:        addt/sud $f16, $f17, $f0

define i64 @cmp(double %a, double %b) {
  %c = fcmp oeq double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}
; A compare takes the software-completion qualifier but never a rounding one.
; NONE-LABEL: cmp:
; NONE:       cmpteq $f16
; SU-LABEL:   cmp:
; SU:         cmpteq/su $f16
; SUI-LABEL:  cmp:
; SUI:        cmpteq/su $f16
; U-LABEL:    cmp:
; U:          cmpteq $f16
; DYN-LABEL:  cmp:
; DYN:        cmpteq $f16
; SUD-LABEL:  cmp:
; SUD:        cmpteq/su $f16

define i64 @ftoi(double %a) {
  %r = fptosi double %a to i64
  ret i64 %r
}
; Float-to-int always chops -- C conversion truncates, and there is no dynamic
; rounding to defer to -- so the rounding modes leave it alone and only the
; trap qualifier changes.  /v is the integer-overflow trap, which is what the
; conversion can raise; the software-completion forms add /s.
; NONE-LABEL: ftoi:
; NONE:       cvttq/c $f16
; SU-LABEL:   ftoi:
; SU:         cvttq/svc $f16
; SUI-LABEL:  ftoi:
; SUI:        cvttq/svic $f16
; U-LABEL:    ftoi:
; U:          cvttq/vc $f16
; DYN-LABEL:  ftoi:
; DYN:        cvttq/c $f16
; SUD-LABEL:  ftoi:
; SUD:        cvttq/svc $f16

define double @itof(i64 %a) {
  %r = sitofp i64 %a to double
  ret double %r
}
; Integer-to-float can only be inexact, and it does round.
; NONE-LABEL: itof:
; NONE:       cvtqt $f
; SU-LABEL:   itof:
; SU:         cvtqt $f
; SUI-LABEL:  itof:
; SUI:        cvtqt/sui $f
; U-LABEL:    itof:
; U:          cvtqt $f
; DYN-LABEL:  itof:
; DYN:        cvtqt/d $f
; SUD-LABEL:  itof:
; SUD:        cvtqt/d $f
