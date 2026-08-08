; RUN: llc -march=alpha -mattr=+ieee < %s | FileCheck %s
;
; f128 (X_floating) type conversions via Alpha OTS routines.

target triple = "alpha-unknown-linux-gnu"

; f64 -> f128
; CHECK-LABEL: fpext_f128:
; CHECK: ldq $27, _OtsConvertFloatTX($29)
define void @fpext_f128(ptr sret(fp128) %ret, double %a) {
  %r = fpext double %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; f128 -> f64
; CHECK-LABEL: fpround_f128:
; CHECK: ldq $27, _OtsConvertFloatXT($29)
define double @fpround_f128(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptrunc fp128 %av to double
  ret double %r
}

; f32 -> f128: widen f32 to f64 in hardware (cvtst, exact), then call OTS.
; CHECK-LABEL: fpext_f32_f128:
; CHECK: cvtst
; CHECK: ldq $27, _OtsConvertFloatTX($29)
define void @fpext_f32_f128(ptr sret(fp128) %ret, float %a) {
  %r = fpext float %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; f128 -> f32: call OTS to get f64, then truncate to f32 in hardware (cvtts).
; CHECK-LABEL: fptrunc_f128_f32:
; CHECK: ldq $27, _OtsConvertFloatXT($29)
; CHECK: cvtts
define float @fptrunc_f128_f32(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptrunc fp128 %av to float
  ret float %r
}

; i64 -> f128
; CHECK-LABEL: sitofp_f128:
; CHECK: ldq $27, _OtsCvtQX($29)
define void @sitofp_f128(ptr sret(fp128) %ret, i64 %a) {
  %r = sitofp i64 %a to fp128
  store fp128 %r, ptr %ret
  ret void
}

; f128 -> i64
; CHECK-LABEL: fptosi_f128:
; CHECK: ldq $27, _OtsCvtXQ($29)
define i64 @fptosi_f128(ptr byref(fp128) %a) {
  %av = load fp128, ptr %a
  %r = fptosi fp128 %av to i64
  ret i64 %r
}
