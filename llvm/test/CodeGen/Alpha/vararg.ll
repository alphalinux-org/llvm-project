; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A variadic function saves the unnamed integer ($16-$21) and floating-point
; ($f16-$f21) argument registers to a save area, and va_start records the base
; and initial offset of the va_list.

declare void @llvm.va_start(ptr)

; CHECK-LABEL: f:
; The unnamed floating-point argument registers are spilled to the save area.
; CHECK-DAG:   stt $f17,
; CHECK-DAG:   stt $f18,
; CHECK-DAG:   stt $f19,
; CHECK-DAG:   stt $f20,
; CHECK-DAG:   stt $f21,
; The unnamed integer argument registers are spilled too -- all of them, not
; just the top pair.
; CHECK-DAG:   stq $21,
; CHECK-DAG:   stq $20,
; CHECK-DAG:   stq $19,
; CHECK-DAG:   stq $18,
; $f16 shares its slot with $16, which holds the named argument, so it is not an
; unnamed register and must not be spilled.
; CHECK-NOT:   stt $f16,
; CHECK:       ret
define i64 @f(i32 %n, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i64
  ret i64 %v
}

; With more than six named arguments the save area must still sit at a fixed
; -48 from the incoming stack pointer, so that slot N is at base + N*8 for
; every N and the named stack arguments line up with slots 6 onwards.  Sizing
; the base by the named arguments' stack usage double-counts them and makes
; va_arg read past the first variadic argument.
;
; Incoming $sp is $30+112 here, so the save area base is $30+64.  The eight
; named arguments occupy slots 0-7, putting the first variadic argument at
; slot 8 == base+64 == $30+128.
; CHECK-LABEL: many:
; CHECK:       lda $30, -112($30)
; CHECK:       ldq {{\$[0-9]+}}, 128($30)
define i64 @many(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g,
                 i64 %h, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i64
  ret i64 %v
}

; A floating-point va_arg reads the floating half of the save area, which sits
; 48 bytes below the base the va_list records: slot N is at base + N*8 - 48.
; Here the one named argument takes slot 0, so the first variadic argument is
; slot 1, read from base + 8 - 48 == $30+24 -- the slot $f17 was spilled to.
; The load out of it folds back into the register that was spilled there.
; CHECK-LABEL: double_arg:
; CHECK-DAG:   cpys $f17, $f17, $f0
; CHECK-DAG:   stt {{\$f[0-9]+}}, 24($30)
; CHECK-DAG:   lda [[BASE:\$[0-9]+]], 64($30)
; CHECK-DAG:   stq [[BASE]], 0($30)
; The offset the va_list is left holding has moved on by one slot.
; CHECK-DAG:   lda [[OFF:\$[0-9]+]], 16($31)
; CHECK-DAG:   stl [[OFF]], 8($30)
define double @double_arg(i32 %n, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, double
  ret double %v
}

; A float argument is promoted to double when it is passed, so it comes out of
; the same slot and is narrowed on the way to the result.
; CHECK-LABEL: float_arg:
; CHECK-DAG:   stt $f17, 24($30)
; CHECK-DAG:   cvtts $f17, $f0
define float @float_arg(i32 %n, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, float
  ret float %v
}

; X_floating is passed as a pair of quadwords in the integer registers, so its
; va_arg reads two slots from the integer half of the save area -- not the
; floating half -- and moves the offset on by two.  The result is returned
; indirectly through $16, which pushes the named argument to slot 1 and the
; first variadic argument to slot 2, at base + 16 == $30+80.
; CHECK-LABEL: fp128_arg:
; CHECK-DAG:   stq $18, 80($30)
; CHECK-DAG:   stq $19, 88($30)
; CHECK-DAG:   ldq {{\$[0-9]+}}, 80($30)
; CHECK-DAG:   ldq {{\$[0-9]+}}, 88($30)
; CHECK-DAG:   lda [[OFF2:\$[0-9]+]], 32($31)
; CHECK-DAG:   stl [[OFF2]], 8($30)
define fp128 @fp128_arg(i32 %n, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, fp128
  ret fp128 %v
}
