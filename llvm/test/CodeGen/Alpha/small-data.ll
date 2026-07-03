; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s \
; RUN:   | FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data < %s \
; RUN:   | FileCheck %s --check-prefix=SMALL

@g = global i64 42

; By default (large-data) a global's address is loaded from the GOT.
; LARGE-LABEL: get:
; LARGE:       ldq $0, g($29){{.*}}!literal
; LARGE:       ldq $0, 0($0)

; With small-data the address is formed GP-relative and the !gprellow low part
; is folded into the load.
; SMALL-LABEL: get:
; SMALL:       ldah $0, g($29){{.*}}!gprelhigh
; SMALL:       ldq $0, g($0){{.*}}!gprellow
define i64 @get() {
  %v = load i64, ptr @g
  ret i64 %v
}

; SMALL-LABEL: set:
; SMALL:       ldah $0, g($29){{.*}}!gprelhigh
; SMALL:       stq $16, g($0){{.*}}!gprellow
define void @set(i64 %x) {
  store i64 %x, ptr @g
  ret void
}

; Six more of the folds the multiclass generates, beside the i64 load and store
; above: the three i32 load forms through LDLg, STLg (truncstorei32) and the two
; floating stores.  The floating loads it also generates reach their patterns
; through the same AlphaGprelLo operand and are not repeated here.
@i32g = global i32 0
@f32g = global float 0.0
@f64g = global double 0.0

; SMALL-LABEL: fold_ldl:
; SMALL:       ldah $0, i32g($29){{.*}}!gprelhigh
; SMALL:       ldl $0, i32g($0){{.*}}!gprellow
define i64 @fold_ldl() {
  %v = load i32, ptr @i32g
  %s = sext i32 %v to i64
  ret i64 %s
}

; SMALL-LABEL: fold_stl:
; SMALL:       ldah $0, i32g($29){{.*}}!gprelhigh
; SMALL:       stl $16, i32g($0){{.*}}!gprellow
define void @fold_stl(i64 %v) {
  %t = trunc i64 %v to i32
  store i32 %t, ptr @i32g
  ret void
}

; SMALL-LABEL: fold_sts:
; SMALL:       ldah $0, f32g($29){{.*}}!gprelhigh
; SMALL:       sts $f16, f32g($0){{.*}}!gprellow
define void @fold_sts(float %v) {
  store float %v, ptr @f32g
  ret void
}

; SMALL-LABEL: fold_stt:
; SMALL:       ldah $0, f64g($29){{.*}}!gprelhigh
; SMALL:       stt $f16, f64g($0){{.*}}!gprellow
define void @fold_stt(double %v) {
  store double %v, ptr @f64g
  ret void
}
