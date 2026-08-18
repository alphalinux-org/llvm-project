; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee,+fpround-dynamic < %s \
; RUN:   | FileCheck %s --check-prefix=DYN
;
; f128 (X_floating) arithmetic: verify that each binary operation lowers to
; the corresponding Alpha OTS routine rather than a generic soft-float call.
; The OTS ABI passes lo/hi halves of each f128 in $16-$19, the round constant
; in $20, and returns the result lo/hi in $16/$17.  The mode is the ambient
; -mfp-rounding-mode -- 2 for the default nearest, 4 for dynamic -- so it has
; to reach f128 arithmetic and not only f128 conversion.

target triple = "alpha-unknown-linux-gnu"

; CHECK-LABEL: add_f128:
; CHECK-DAG: ldq $27, _OtsAddX($29)
; CHECK-DAG: lda $20, 2($31)
; CHECK:     jsr $26, ($27)
; DYN-LABEL: add_f128:
; DYN-DAG:   ldq $27, _OtsAddX($29)
; DYN-DAG:   lda $20, 4($31)
define void @add_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fadd fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: sub_f128:
; CHECK-DAG: ldq $27, _OtsSubX($29)
; CHECK-DAG: lda $20, 2($31)
; CHECK:     jsr $26, ($27)
; DYN-LABEL: sub_f128:
; DYN-DAG:   ldq $27, _OtsSubX($29)
; DYN-DAG:   lda $20, 4($31)
define void @sub_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fsub fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: mul_f128:
; CHECK-DAG: ldq $27, _OtsMulX($29)
; CHECK-DAG: lda $20, 2($31)
; CHECK:     jsr $26, ($27)
; DYN-LABEL: mul_f128:
; DYN-DAG:   ldq $27, _OtsMulX($29)
; DYN-DAG:   lda $20, 4($31)
define void @mul_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fmul fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: div_f128:
; CHECK-DAG: ldq $27, _OtsDivX($29)
; CHECK-DAG: lda $20, 2($31)
; CHECK:     jsr $26, ($27)
; DYN-LABEL: div_f128:
; DYN-DAG:   ldq $27, _OtsDivX($29)
; DYN-DAG:   lda $20, 4($31)
define void @div_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fdiv fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; Strict (constrained) variants go through the same OTS routines.
; CHECK-LABEL: add_f128_strict:
; CHECK-DAG: ldq $27, _OtsAddX($29)
; CHECK-DAG: lda $20, 2($31)
; CHECK:     jsr $26, ($27)
define void @add_f128_strict(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) strictfp {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = call fp128 @llvm.experimental.constrained.fadd.f128(fp128 %av, fp128 %bv,
                    metadata !"round.tonearest", metadata !"fpexcept.strict")
  store fp128 %r, ptr %ret
  ret void
}

declare fp128 @llvm.experimental.constrained.fadd.f128(fp128, fp128, metadata, metadata) strictfp
