; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee,+fpround-chopped < %s \
; RUN:   | FileCheck %s --check-prefix=CHOP
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s \
; RUN:   --check-prefix=NOIEEE
;
; f128 (X_floating) type conversions via Alpha OTS routines.
;
; The routines that round take the rounding mode in $18: 0 chopped, 1 minus,
; 2 normal, 4 dynamic, as _OtsConvertFloatXT and friends read it.  The ones
; that cannot round -- widening to X_floating, and integer to X_floating --
; take no mode argument at all.

target triple = "alpha-unknown-linux-gnu"

; f64 -> f128: exact, so no rounding mode is passed.
; CHECK-LABEL: fpext_f128:
; CHECK:     ldq $27, _OtsConvertFloatTX($29)
; CHECK-NOT: $18
define void @fpext_f128(ptr sret(fp128) %ret, double %a) {
  %r = fpext double %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; f128 -> f64 rounds, so it follows -mfp-rounding-mode: normal by default.
; CHECK-LABEL: fpround_f128:
; CHECK-DAG: ldq $27, _OtsConvertFloatXT($29)
; CHECK-DAG: lda $18, 2($31)
; CHOP-LABEL: fpround_f128:
; CHOP-DAG:   ldq $27, _OtsConvertFloatXT($29)
; CHOP-DAG:   lda $18, 0($31)
; Without -mieee or -mfp-trap-mode=u, gcc's alpha_emit_xfloating_cvt sets bit
; 16 of the mode argument for a narrowing conversion, so the constant is
; 0x10002 and takes an ldah as well.
; NOIEEE-LABEL: fpround_f128:
; NOIEEE-DAG:   ldq $27, _OtsConvertFloatXT($29)
; NOIEEE-DAG:   ldah [[H:\$[0-9]+]], 1($31)
; NOIEEE-DAG:   lda $18, 2([[H]])
define double @fpround_f128(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptrunc fp128 %av to double
  ret double %r
}

; f32 -> f128: widen f32 to f64 in hardware (cvtst, exact), then call OTS.
; CHECK-LABEL: fpext_f32_f128:
; CHECK: cvtst
; CHECK: ldq $27, _OtsConvertFloatTX($29)
define void @fpext_f32_f128(ptr sret(fp128) %ret, float %a) {
  %r = fpext float %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; f128 -> f32: call OTS to get f64, then truncate to f32 in hardware (cvtts).
; CHECK-LABEL: fptrunc_f128_f32:
; CHECK: ldq $27, _OtsConvertFloatXT($29)
; CHECK: cvtts
define float @fptrunc_f128_f32(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptrunc fp128 %av to float
  ret float %r
}

; i64 -> f128 is exact for the same reason, and passes no mode either.
; CHECK-LABEL: sitofp_f128:
; CHECK:     ldq $27, _OtsCvtQX($29)
; CHECK-NOT: $18
define void @sitofp_f128(ptr sret(fp128) %ret, i64 %a) {
  %r = sitofp i64 %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; C requires a conversion to integer to truncate toward zero whatever the
; ambient rounding mode is, so this one is chopped in every configuration.
; CHECK-LABEL: fptosi_f128:
; CHECK-DAG:  ldq $27, _OtsCvtXQ($29)
; CHECK-DAG:  lda $18, 0($31)
; CHOP-LABEL: fptosi_f128:
; CHOP-DAG:   lda $18, 0($31)
; NOIEEE-LABEL: fptosi_f128:
; NOIEEE-DAG:   lda $18, 0($31)
define i64 @fptosi_f128(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptosi fp128 %av to i64
  ret i64 %r
}

; The unsigned conversion calls the same routine.  OTS has no unsigned
; X_floating-to-quadword entry point, and gcc's alpha_emit_xfloating_cvt
; rewrites UNSIGNED_FIX to FIX before looking one up, so a value at or above
; 2^63 converts the same (wrong) way it does with gcc.  Pinning that here keeps
; it a deliberate choice rather than something to be re-discovered.
; CHECK-LABEL: fptoui_f128:
; CHECK-DAG:  ldq $27, _OtsCvtXQ($29)
; CHECK-DAG:  lda $18, 0($31)
define i64 @fptoui_f128(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptoui fp128 %av to i64
  ret i64 %r
}
