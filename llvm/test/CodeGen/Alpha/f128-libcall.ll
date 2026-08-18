; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
;
; The libm entry points behind the f128 intrinsics -- floorl, fmal, fmaxl,
; sinl, ... -- are ordinary C functions taking long double, so they follow the
; OSF rule for X_floating: each argument is passed by invisible reference, a
; pointer to a 16-byte slot, and the result comes back through the hidden
; pointer in $16.  Softening f128 to a pair of integers and passing that pair
; in $16-$21, as the generic path does, makes glibc dereference the low half of
; the value as an address.
;
; This is the opposite of the _Ots* arithmetic runtime (f128-arith.ll), which
; really does take X_floating by value; the last test here pins that contrast.

target triple = "alpha-unknown-linux-gnu"

; One argument: $16 is the hidden return pointer, $17 the address of a slot
; this frame owns, and the two halves are stored into that slot beforehand.
; CHECK-LABEL: floor_f128:
; CHECK-DAG: stq {{\$[0-9]+}}, 16($30)
; CHECK-DAG: stq {{\$[0-9]+}}, 24($30)
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, 16($30)
; CHECK:     ldq $27, floorl($29)
define fp128 @floor_f128(fp128 %a) {
  %r = call fp128 @llvm.floor.f128(fp128 %a)
  ret fp128 %r
}

; Three arguments, so three slots and three addresses; none of $17-$19 holds a
; piece of a value.
; CHECK-LABEL: fma_f128:
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, {{[0-9]+}}($30)
; CHECK-DAG: lda $18, {{[0-9]+}}($30)
; CHECK-DAG: lda $19, {{[0-9]+}}($30)
; CHECK:     ldq $27, fmal($29)
define fp128 @fma_f128(fp128 %a, fp128 %b, fp128 %c) {
  %r = call fp128 @llvm.fma.f128(fp128 %a, fp128 %b, fp128 %c)
  ret fp128 %r
}

; CHECK-LABEL: maxnum_f128:
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, {{[0-9]+}}($30)
; CHECK-DAG: lda $18, {{[0-9]+}}($30)
; CHECK:     ldq $27, fmaxl($29)
define fp128 @maxnum_f128(fp128 %a, fp128 %b) {
  %r = call fp128 @llvm.maxnum.f128(fp128 %a, fp128 %b)
  ret fp128 %r
}

; CHECK-LABEL: sin_f128:
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, {{[0-9]+}}($30)
; CHECK:     ldq $27, sinl($29)
define fp128 @sin_f128(fp128 %a) {
  %r = call fp128 @llvm.sin.f128(fp128 %a)
  ret fp128 %r
}

; CHECK-LABEL: rem_f128:
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, {{[0-9]+}}($30)
; CHECK-DAG: lda $18, {{[0-9]+}}($30)
; CHECK:     ldq $27, fmodl($29)
define fp128 @rem_f128(fp128 %a, fp128 %b) {
  %r = frem fp128 %a, %b
  ret fp128 %r
}

; powil's exponent is an ordinary int and stays in a register; only the
; X_floating argument becomes a pointer.
; CHECK-LABEL: powi_f128:
; CHECK-DAG: lda $16, 0($30)
; CHECK-DAG: lda $17, {{[0-9]+}}($30)
; CHECK:     ldq $27, __powitf2($29)
define fp128 @powi_f128(fp128 %a, i32 %n) {
  %r = call fp128 @llvm.powi.f128.i32(fp128 %a, i32 %n)
  ret fp128 %r
}

; The OTS runtime is the exception: _OtsAddX takes the two halves of each
; X_floating by value in $16-$19 and returns them in $16/$17.  No slot address
; is built for it.
; CHECK-LABEL: add_f128:
; CHECK-NOT: lda $17, {{[0-9]+}}($30)
; CHECK:     jsr $26, ($27)
define fp128 @add_f128(fp128 %a, fp128 %b) {
  %r = fadd fp128 %a, %b
  ret fp128 %r
}
