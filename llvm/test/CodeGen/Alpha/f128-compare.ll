; RUN: llc -march=alpha -mattr=+ieee < %s | FileCheck %s
;
; f128 (X_floating) comparisons: ordered conditions map to the _Ots routine
; directly; unordered conditions map to the complement (ordered opposite) with
; the result negated via XOR 1.

target triple = "alpha-unknown-linux-gnu"

; CHECK-LABEL: lt_f128:
; CHECK: ldq $27, _OtsLssX($29)
define i1 @lt_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp olt fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: le_f128:
; CHECK: ldq $27, _OtsLeqX($29)
define i1 @le_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ole fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: gt_f128:
; CHECK: ldq $27, _OtsGtrX($29)
define i1 @gt_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ogt fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ge_f128:
; CHECK: ldq $27, _OtsGeqX($29)
define i1 @ge_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp oge fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: eq_f128:
; CHECK: ldq $27, _OtsEqlX($29)
define i1 @eq_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp oeq fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: one_f128:
; CHECK: ldq $27, _OtsNeqX($29)
define i1 @one_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp one fp128 %av, %bv
  ret i1 %r
}

; SETUNE(a,b) = !SETOEQ(a,b): uses _OtsEqlX + XOR 1.
; CHECK-LABEL: une_f128:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: xor {{.*}}, 1,
define i1 @une_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp une fp128 %av, %bv
  ret i1 %r
}

; Constrained compare (strict FP): same OTS routine, chain threaded.
; CHECK-LABEL: une_f128_strict:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: xor {{.*}}, 1,
define i1 @une_f128_strict(ptr byref(fp128) %a, ptr byref(fp128) %b) strictfp {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = call i1 @llvm.experimental.constrained.fcmp.f128(fp128 %av, fp128 %bv,
                   metadata !"une", metadata !"fpexcept.strict")
  ret i1 %r
}

declare i1 @llvm.experimental.constrained.fcmp.f128(fp128, fp128, metadata, metadata) strictfp
