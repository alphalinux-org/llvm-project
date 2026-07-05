; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: not llc -mtriple=alpha-unknown-linux-gnu -mattr=+no-fp-regs < %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=NOFP

; Integer code is unaffected by -mno-fp-regs.
; CHECK-LABEL: icopy:
; CHECK: ldq $0, 0($17)
; CHECK: stq $0, 0($16)
define void @icopy(ptr %d, ptr %s) {
  %v = load i64, ptr %s
  store i64 %v, ptr %d
  ret void
}

; With the floating-point registers reserved, floating-point code has nowhere to
; allocate -- the kernel is built this way precisely because it performs no
; floating point.
; NOFP: no registers from class available to allocate
define void @dcopy(ptr %d, ptr %s) {
  %v = load double, ptr %s
  store double %v, ptr %d
  ret void
}
