; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s \
; RUN:   | FileCheck %s --check-prefix=NOFP

; -mno-fp-regs keeps the floating-point registers out of use entirely -- the
; kernel is built this way precisely because it performs no floating point.  As
; in GCC, that implies soft float: with no register to hold one, a floating
; value lives in an integer register and every operation on it is a libcall.
;
; That also settles the calling sequence, which GCC documents rather than
; leaves to the calling standard: "If the floating-point register set is not
; used, floating-point operands are passed in integer registers and
; floating-point results are passed in $0 instead of $f0.  This is a
; non-standard calling sequence, so any function with a floating-point argument
; or return value called by code compiled with -mno-fp-regs must also be
; compiled with that option" (gcc/doc/invoke.texi, -mfp-regs).  Softening an
; illegal f64 puts it exactly there without anything being written here: the
; arguments are already in $16 and $17 when the function is entered, and the
; result is already in $0 when the libcall returns.  GCC's own
; alpha_function_arg implements the argument half; its alpha_function_value_1
; does not implement the return half, which is why the compiler ICEs on its own
; flag and is not usable as the oracle here.

; Integer code is unaffected.
; CHECK-LABEL: icopy:
; CHECK: ldq $0, 0($17)
; CHECK: stq $0, 0($16)
; NOFP-LABEL: icopy:
; NOFP: ldq $0, 0($17)
; NOFP: stq $0, 0($16)
define void @icopy(ptr %d, ptr %s) {
  %v = load i64, ptr %s
  store i64 %v, ptr %d
  ret void
}

; Moving a double is moving its bits, which an integer register holds just as
; well; nothing is called for it.
; CHECK-LABEL: dcopy:
; CHECK: ldt $f0, 0($17)
; CHECK: stt $f0, 0($16)
; NOFP-LABEL: dcopy:
; NOFP: ldq $0, 0($17)
; NOFP: stq $0, 0($16)
; NOFP-NOT: $f
define void @dcopy(ptr %d, ptr %s) {
  %v = load double, ptr %s
  store double %v, ptr %d
  ret void
}

; Arithmetic on one becomes a call, with the arguments in the integer argument
; registers rather than $f16 and $f17.
; CHECK-LABEL: dadd:
; CHECK: addt $f16, $f17, $f0
; NOFP-LABEL: dadd:
; NOFP: ldq $27, __adddf3($29)
; NOFP: jsr $26, ($27)
define double @dadd(double %a, double %b) {
  %r = fadd double %a, %b
  ret double %r
}

; A comparison too: there is no cmptlt to use.
; CHECK-LABEL: dcmp:
; CHECK: cmptlt $f16, $f17, $f0
; NOFP-LABEL: dcmp:
; NOFP: ldq $27, __ltdf2($29)
; NOFP: jsr $26, ($27)
define i64 @dcmp(double %a, double %b) {
  %c = fcmp olt double %a, %b
  %r = zext i1 %c to i64
  ret i64 %r
}

; A select is a conditional move on the bits, so it stays a single instruction.
; NOFP-LABEL: dsel:
; NOFP: cmoveq $18, $16, $0
define double @dsel(double %a, double %b, i64 %c) {
  %p = icmp eq i64 %c, 0
  %r = select i1 %p, double %a, double %b
  ret double %r
}

; A variadic function has no floating-point argument registers to save: the
; register file is out of the target's register classes, so f64 is an illegal
; type and a copy out of $f16 could not be legalized at all.  The
; floating-point half of the save area is filled from the integer registers
; instead, which is where a floating-point argument arrives under this flag.
; GCC does the same, from the same register numbers.
; NOFP-LABEL: vsave:
; NOFP-DAG:   stq $18, 32($30)
; NOFP-DAG:   stq $18, 80($30)
; NOFP-DAG:   stq $19, 40($30)
; NOFP-DAG:   stq $19, 88($30)
; NOFP-DAG:   stq $20, 48($30)
; NOFP-DAG:   stq $20, 96($30)
; NOFP-DAG:   stq $21, 56($30)
; NOFP-DAG:   stq $21, 104($30)
; NOFP-NOT:   $f
define i64 @vsave(i32 %n, ...) {
  %ap = alloca [2 x i64], align 8
  call void @llvm.va_start(ptr %ap)
  %v = va_arg ptr %ap, i64
  ret i64 %v
}

declare void @llvm.va_start(ptr)
