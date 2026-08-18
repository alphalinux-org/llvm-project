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
; The high unnamed integer argument registers are spilled too.
; CHECK-DAG:   stq $21,
; CHECK-DAG:   stq $20,
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
