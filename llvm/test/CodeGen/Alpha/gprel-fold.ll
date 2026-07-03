; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; The !gprellow low part folds into the load or store displacement, so reaching
; one of these globals is the ldah !gprelhigh and the access, not three
; instructions.
@i64g = dso_local global i64 42
@i32g = dso_local global i32 0
@f32g = dso_local global float 0.0
@f64g = dso_local global double 0.0

; CHECK-LABEL: fold_ldq:
; CHECK:       ldah $0, i64g($29){{.*}}!gprelhigh
; CHECK:       ldq $0, i64g($0){{.*}}!gprellow
define i64 @fold_ldq() {
  %v = load i64, ptr @i64g
  ret i64 %v
}

; CHECK-LABEL: fold_stq:
; CHECK:       ldah $0, i64g($29){{.*}}!gprelhigh
; CHECK:       stq $16, i64g($0){{.*}}!gprellow
define void @fold_stq(i64 %x) {
  store i64 %x, ptr @i64g
  ret void
}

; Four more of the folds the multiclass generates: the i32 load through LDLg,
; STLg (truncstorei32) and the two floating stores.  The floating loads it also
; generates reach their patterns through the same AlphaGprelLo operand and are
; not repeated here.
; CHECK-LABEL: fold_ldl:
; CHECK:       ldah $0, i32g($29){{.*}}!gprelhigh
; CHECK:       ldl $0, i32g($0){{.*}}!gprellow
define i64 @fold_ldl() {
  %v = load i32, ptr @i32g
  %s = sext i32 %v to i64
  ret i64 %s
}

; CHECK-LABEL: fold_stl:
; CHECK:       ldah $0, i32g($29){{.*}}!gprelhigh
; CHECK:       stl $16, i32g($0){{.*}}!gprellow
define void @fold_stl(i64 %v) {
  %t = trunc i64 %v to i32
  store i32 %t, ptr @i32g
  ret void
}

; CHECK-LABEL: fold_sts:
; CHECK:       ldah $0, f32g($29){{.*}}!gprelhigh
; CHECK:       sts $f16, f32g($0){{.*}}!gprellow
define void @fold_sts(float %v) {
  store float %v, ptr @f32g
  ret void
}

; CHECK-LABEL: fold_stt:
; CHECK:       ldah $0, f64g($29){{.*}}!gprelhigh
; CHECK:       stt $f16, f64g($0){{.*}}!gprellow
define void @fold_stt(double %v) {
  store double %v, ptr @f64g
  ret void
}
