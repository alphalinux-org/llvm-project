; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s
;
; f128 (X_floating) comparisons.  The _Ots routines return -1 unordered, 0
; false and 1 true, so each condition is the routine plus a comparison of its
; result against zero: `> 0' for an ordered condition, `<= 0' for its unordered
; complement, and the sign of _OtsEqlX's result for ord/uno.  The check on that
; comparison is the only thing that separates a correct NaN answer from a wrong
; one, since every condition calls a routine either way.

target triple = "alpha-unknown-linux-gnu"

; An ordered condition is false for a NaN, which is what `0 < result' rejects.
; CHECK-LABEL: lt_f128:
; CHECK: ldq $27, _OtsLssX($29)
; CHECK: cmplt $31, $0, $0
define i1 @lt_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp olt fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: le_f128:
; CHECK: ldq $27, _OtsLeqX($29)
; CHECK: cmplt $31, $0, $0
define i1 @le_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ole fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: gt_f128:
; CHECK: ldq $27, _OtsGtrX($29)
; CHECK: cmplt $31, $0, $0
define i1 @gt_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ogt fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ge_f128:
; CHECK: ldq $27, _OtsGeqX($29)
; CHECK: cmplt $31, $0, $0
define i1 @ge_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp oge fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: eq_f128:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: cmplt $31, $0, $0
define i1 @eq_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp oeq fp128 %av, %bv
  ret i1 %r
}

; _OtsNeqX returns 1 only when the operands are ordered and differ, so `> 0'
; is the ordered not-equal that `one' asks for.
; CHECK-LABEL: one_f128:
; CHECK: ldq $27, _OtsNeqX($29)
; CHECK: cmplt $31, $0, $0
define i1 @one_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp one fp128 %av, %bv
  ret i1 %r
}

; An unordered condition is the complement of the opposite ordered one, so it
; is `result < 1', which is true for the -1 a NaN produces.
; CHECK-LABEL: une_f128:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: cmplt $0, 1, $0
define i1 @une_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp une fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ueq_f128:
; CHECK: ldq $27, _OtsNeqX($29)
; CHECK: cmplt $0, 1, $0
define i1 @ueq_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ueq fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ult_f128:
; CHECK: ldq $27, _OtsGeqX($29)
; CHECK: cmplt $0, 1, $0
define i1 @ult_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ult fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ule_f128:
; CHECK: ldq $27, _OtsGtrX($29)
; CHECK: cmplt $0, 1, $0
define i1 @ule_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ule fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: ugt_f128:
; CHECK: ldq $27, _OtsLeqX($29)
; CHECK: cmplt $0, 1, $0
define i1 @ugt_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ugt fp128 %av, %bv
  ret i1 %r
}

; CHECK-LABEL: uge_f128:
; CHECK: ldq $27, _OtsLssX($29)
; CHECK: cmplt $0, 1, $0
define i1 @uge_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp uge fp128 %av, %bv
  ret i1 %r
}

; Constrained compare (strict FP): same OTS routine, chain threaded.
; CHECK-LABEL: une_f128_strict:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: cmplt $0, 1, $0
define i1 @une_f128_strict(ptr byref(fp128) %a, ptr byref(fp128) %b) strictfp {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = call i1 @llvm.experimental.constrained.fcmp.f128(fp128 %av, fp128 %bv,
                   metadata !"une", metadata !"fpexcept.strict")
  ret i1 %r
}

declare i1 @llvm.experimental.constrained.fcmp.f128(fp128, fp128, metadata, metadata) strictfp

; _OtsEqlX returns -1 exactly when an operand is a NaN, so the sign of its
; result is the unordered predicate on its own.
; CHECK-LABEL: uo_f128:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: srl $0, 63, $0
define i1 @uo_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp uno fp128 %av, %bv
  ret i1 %r
}

; And ord is the same result being non-negative.
; CHECK-LABEL: o_f128:
; CHECK: ldq $27, _OtsEqlX($29)
; CHECK: lda $1, -1($31)
; CHECK: cmplt $1, $0, $0
define i1 @o_f128(ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = fcmp ord fp128 %av, %bv
  ret i1 %r
}

; An icmp on an extended (non-simple) integer type reaches the same f128
; combine hook.  It must bail out on the EVT rather than assert in
; getSimpleValueType().  There is nothing to check about the code it produces --
; the generic path handles it -- so this is a crash test: reaching the label at
; all is the property.
; CHECK-LABEL: icmp_i65:
define i1 @icmp_i65(i64 %n) {
  %w = zext i64 %n to i65
  %o = call { i65, i1 } @llvm.sadd.with.overflow.i65(i65 %w, i65 32)
  %v = extractvalue { i65, i1 } %o, 0
  %t = trunc i65 %v to i64
  %s = sext i64 %t to i65
  %r = icmp ne i65 %v, %s
  ret i1 %r
}
