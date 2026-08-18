; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee < %s | FileCheck %s
;
; f128 (X_floating) bitwise sign operations: implemented via integer ops on
; the high word without any OTS call.

target triple = "alpha-unknown-linux-gnu"

; FNEG: XOR high word with the sign mask (bit 63).
; CHECK-LABEL: fneg_f128:
; CHECK-NOT: jsr
; CHECK: xor
define void @fneg_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fneg fp128 %av
  store fp128 %r, ptr %ret
  ret void
}

; FABS: AND high word with ~sign mask to clear bit 63.
; CHECK-LABEL: fabs_f128:
; CHECK-NOT: jsr
; CHECK: and
define void @fabs_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = call fp128 @llvm.fabs.f128(fp128 %av)
  store fp128 %r, ptr %ret
  ret void
}

; FCOPYSIGN: magnitude from op0 hi, sign bit from op1 hi.
; Both halves have to be checked: with only the CHECK-NOT this passed for any
; implementation that avoided a call, including one that dropped a term.
; CHECK-LABEL: fcopysign_f128:
; CHECK-NOT: jsr
; CHECK:     and ${{[0-9]+}}, ${{[0-9]+}}, ${{[0-9]+}}
; CHECK:     and ${{[0-9]+}}, ${{[0-9]+}}, ${{[0-9]+}}
; CHECK:     bis ${{[0-9]+}}, ${{[0-9]+}}, ${{[0-9]+}}
define void @fcopysign_f128(ptr sret(fp128) %ret, ptr byref(fp128) %a, ptr byref(fp128) %b) {
  %av = load fp128, ptr %a
  %bv = load fp128, ptr %b
  %r = call fp128 @llvm.copysign.f128(fp128 %av, fp128 %bv)
  store fp128 %r, ptr %ret
  ret void
}

declare fp128 @llvm.fabs.f128(fp128)
declare fp128 @llvm.copysign.f128(fp128, fp128)
