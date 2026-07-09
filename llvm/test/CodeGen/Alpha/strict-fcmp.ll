; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; Alpha signals no per-instruction floating-point exception, so a constrained
; compare of a native float type lowers to the ordinary compare.

; CHECK-LABEL: strict_olt_f64:
; CHECK: cmptlt $f16, $f17,
define i64 @strict_olt_f64(double %a, double %b) strictfp {
  %c = call i1 @llvm.experimental.constrained.fcmp.f64(double %a, double %b, metadata !"olt", metadata !"fpexcept.strict")
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: strict_oeq_f32:
; CHECK: cmpteq $f16, $f17,
define i64 @strict_oeq_f32(float %a, float %b) strictfp {
  %c = call i1 @llvm.experimental.constrained.fcmps.f32(float %a, float %b, metadata !"oeq", metadata !"fpexcept.strict")
  %r = zext i1 %c to i64
  ret i64 %r
}

declare i1 @llvm.experimental.constrained.fcmp.f64(double, double, metadata, metadata)
declare i1 @llvm.experimental.constrained.fcmps.f32(float, float, metadata, metadata)
