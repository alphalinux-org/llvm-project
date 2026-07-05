; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data < %s | FileCheck %s

; With -msmall-data, a small locally-defined global is placed in .sdata/.sbss
; and addressed relative to the global pointer; large, external, or explicitly
; sectioned globals stay in the GOT.
;
; Placement and addressing are separate questions.  A preemptible definition is
; still small enough for .sdata, but its address is whatever the dynamic linker
; picks, so it has to come from the GOT.  gcc does the same: built
; -fPIC -msmall-data, a default-visibility global lands in .sbss and is still
; loaded with !literal.

@small = dso_local global i64 7
@smallbss = dso_local global i64 0
@big = dso_local global [64 x i64] zeroinitializer
@ext = external global i64
@preempt = global i64 7

; CHECK-LABEL: get_small:
; CHECK: ldah $0, small($29) !gprelhigh
; CHECK: lda $0, small($0) !gprellow
define ptr @get_small() {
  ret ptr @small
}

; Size decides placement, not addressing: @big is too large for .sdata but is
; still a fixed distance from gp, so its address is built the same way.
; CHECK-LABEL: get_big:
; CHECK: ldah $0, big($29) !gprelhigh
; CHECK: lda $0, big($0) !gprellow
define ptr @get_big() {
  ret ptr @big
}

; CHECK-LABEL: get_ext:
; CHECK: ldq $0, ext($29) !literal
define ptr @get_ext() {
  ret ptr @ext
}

; Small enough for .sdata, but preemptible, so the address comes from the GOT.
; CHECK-LABEL: get_preempt:
; CHECK: ldq $0, preempt($29) !literal
; CHECK-NOT: gprelhigh
define ptr @get_preempt() {
  ret ptr @preempt
}

; Six more of the folds the multiclass generates, beside the i64 load and store
; above: the three i32 load forms through LDLg, STLg (truncstorei32) and the two
; floating stores.  The floating loads it also generates reach their patterns
; through the same AlphaGprelLo operand and are not repeated here.
@i32g = dso_local global i32 0
@f32g = dso_local global float 0.0
@f64g = dso_local global double 0.0

; CHECK-LABEL: fold_ldl:
; CHECK:       ldah $0, i32g($29) !gprelhigh
; CHECK:       ldl $0, i32g($0) !gprellow
define i64 @fold_ldl() {
  %v = load i32, ptr @i32g
  %s = sext i32 %v to i64
  ret i64 %s
}

; A longword load that is not sign-extending folds too: an extending load is the
; same instruction, and a zero-extending one is that instruction plus a zapnot.
; CHECK-LABEL: fold_ldl_ext:
; CHECK:       ldah $0, i32g($29) !gprelhigh
; CHECK:       ldl $0, i32g($0) !gprellow
define i32 @fold_ldl_ext() {
  %v = load i32, ptr @i32g
  ret i32 %v
}

; CHECK-LABEL: fold_ldl_zext:
; CHECK:       ldah $0, i32g($29) !gprelhigh
; CHECK:       ldl $0, i32g($0) !gprellow
; CHECK:       zapnot $0, 15, $0
define i64 @fold_ldl_zext() {
  %v = load i32, ptr @i32g
  %z = zext i32 %v to i64
  ret i64 %z
}

; CHECK-LABEL: fold_stl:
; CHECK:       ldah $0, i32g($29) !gprelhigh
; CHECK:       stl $16, i32g($0) !gprellow
define void @fold_stl(i64 %v) {
  %t = trunc i64 %v to i32
  store i32 %t, ptr @i32g
  ret void
}

; CHECK-LABEL: fold_sts:
; CHECK:       ldah $0, f32g($29) !gprelhigh
; CHECK:       sts $f16, f32g($0) !gprellow
define void @fold_sts(float %v) {
  store float %v, ptr @f32g
  ret void
}

; CHECK-LABEL: fold_stt:
; CHECK:       ldah $0, f64g($29) !gprelhigh
; CHECK:       stt $f16, f64g($0) !gprellow
define void @fold_stt(double %v) {
  store double %v, ptr @f64g
  ret void
}

; CHECK: .section .sdata,"aw",@progbits
; CHECK: small:
; CHECK: .section .sbss,"aw",@nobits
; CHECK: smallbss:
; @big stays in a regular data section, not .sdata.
; CHECK: .section .bss
; CHECK: big:
; Being preemptible does not keep a small global out of .sdata; it only decides
; how its address is formed.
; CHECK: .section .sdata,"aw",@progbits
; CHECK: preempt:
