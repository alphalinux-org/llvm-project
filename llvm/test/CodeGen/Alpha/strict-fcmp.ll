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

; The two intrinsics differ in whether a quiet compare may signal, and the two
; types take different instructions, so each pairing needs its own case: f64
; used only the quiet form and f32 only the signalling one, leaving the other
; half of each axis untested.
; CHECK-LABEL: strict_olt_f64_signalling:
; CHECK: cmptlt $f16, $f17,
define i64 @strict_olt_f64_signalling(double %a, double %b) strictfp {
  %c = call i1 @llvm.experimental.constrained.fcmps.f64(double %a, double %b, metadata !"olt", metadata !"fpexcept.strict")
  %r = zext i1 %c to i64
  ret i64 %r
}

; CHECK-LABEL: strict_oeq_f32_quiet:
; CHECK: cmpteq $f16, $f17,
define i64 @strict_oeq_f32_quiet(float %a, float %b) strictfp {
  %c = call i1 @llvm.experimental.constrained.fcmp.f32(float %a, float %b, metadata !"oeq", metadata !"fpexcept.strict")
  %r = zext i1 %c to i64
  ret i64 %r
}

declare i1 @llvm.experimental.constrained.fcmps.f64(double, double, metadata, metadata)
declare i1 @llvm.experimental.constrained.fcmp.f32(float, float, metadata, metadata)

declare i1 @llvm.experimental.constrained.fcmp.f64(double, double, metadata, metadata)
declare i1 @llvm.experimental.constrained.fcmps.f32(float, float, metadata, metadata)
