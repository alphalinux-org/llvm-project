; RUN: llc -march=alpha -mattr=+ieee < %s | FileCheck %s
;
; f128 (X_floating) arithmetic: verify that each binary operation lowers to
; the corresponding Alpha OTS routine rather than a generic soft-float call.
; The OTS ABI passes lo/hi halves of each f128 in $16-$19, the round constant
; (2 = nearest) in $20, and returns the result lo/hi in $16/$17.

target triple = "alpha-unknown-linux-gnu"

; CHECK-LABEL: add_f128:
; CHECK: ldq $27, _OtsAddX($29)
; CHECK: jsr $26, ($27)
define void @add_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fadd fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: sub_f128:
; CHECK: ldq $27, _OtsSubX($29)
; CHECK: jsr $26, ($27)
define void @sub_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fsub fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: mul_f128:
; CHECK: ldq $27, _OtsMulX($29)
; CHECK: jsr $26, ($27)
define void @mul_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fmul fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; CHECK-LABEL: div_f128:
; CHECK: ldq $27, _OtsDivX($29)
; CHECK: jsr $26, ($27)
define void @div_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fdiv fp128 %av, %bv
  store fp128 %r, ptr %ret
  ret void
}

; Strict (constrained) variants go through the same OTS routines.
; CHECK-LABEL: add_f128_strict:
; CHECK: ldq $27, _OtsAddX($29)
; CHECK: jsr $26, ($27)
define void @add_f128_strict(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) strictfp {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = call fp128 @llvm.experimental.constrained.fadd.f128(fp128 %av, fp128 %bv,
                    metadata !"round.tonearest", metadata !"fpexcept.strict")
  store fp128 %r, ptr %ret
  ret void
}

declare fp128 @llvm.experimental.constrained.fadd.f128(fp128, fp128, metadata, metadata) strictfp
