; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NORM
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fpround-chopped < %s | FileCheck %s --check-prefix=CHOP
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fpround-minus < %s | FileCheck %s --check-prefix=MINF
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fpround-dynamic < %s | FileCheck %s --check-prefix=DYN
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s --check-prefix=IEEE

; The OTS X_floating routines take a mode argument in $18: chopped 0, toward
; minus infinity 1, to nearest 2, dynamic 4, plus bit 16 for a narrowing
; conversion when the trap mode is the default one.  These are the values gcc's
; alpha_compute_xfloating_mode_arg produces, checked against gcc 16.2.

; A conversion to integer must truncate whatever the rounding mode is, so it
; always passes 0 -- gcc's alpha_emit_xfloating_cvt hardcodes ALPHA_FPRM_CHOP
; for a FIX and reads the rounding mode only for a FLOAT_TRUNCATE.
; NORM-LABEL: to_int:
; NORM:       lda $18, 0($31)
; CHOP-LABEL: to_int:
; CHOP:       lda $18, 0($31)
; MINF-LABEL: to_int:
; MINF:       lda $18, 0($31)
; DYN-LABEL:  to_int:
; DYN:        lda $18, 0($31)
; IEEE-LABEL: to_int:
; IEEE:       lda $18, 0($31)
define i64 @to_int(fp128 %x) {
  %r = fptosi fp128 %x to i64
  ret i64 %r
}

; A narrowing conversion rounds, so it does follow -mfp-rounding-mode.  Bit 16
; is set unless a trap mode was asked for, which is why the +ieee run differs.
; NORM-LABEL: to_double:
; NORM:       ldah $[[R:[0-9]+]], 1($31)
; NORM:       lda $18, 2($[[R]])
; CHOP-LABEL: to_double:
; CHOP:       ldah $18, 1($31)
; MINF-LABEL: to_double:
; MINF:       ldah $[[R:[0-9]+]], 1($31)
; MINF:       lda $18, 1($[[R]])
; DYN-LABEL:  to_double:
; DYN:        ldah $[[R:[0-9]+]], 1($31)
; DYN:        lda $18, 4($[[R]])
; IEEE-LABEL: to_double:
; IEEE:       lda $18, 2($31)
define double @to_double(fp128 %x) {
  %r = fptrunc fp128 %x to double
  ret double %r
}

; Arithmetic takes the mode too, in $20: gcc's alpha_emit_xfloating_arith
; passes alpha_compute_xfloating_mode_arg (code, alpha_fprm), so
; -mfp-rounding-mode reaches an f128 add as much as it reaches a conversion.
; NORM-LABEL: add:
; NORM:       lda $20, 2($31)
; CHOP-LABEL: add:
; CHOP:       lda $20, 0($31)
; MINF-LABEL: add:
; MINF:       lda $20, 1($31)
; DYN-LABEL:  add:
; DYN:        lda $20, 4($31)
; IEEE-LABEL: add:
; IEEE:       lda $20, 2($31)
define fp128 @add(fp128 %a, fp128 %b) {
  %r = fadd fp128 %a, %b
  ret fp128 %r
}
