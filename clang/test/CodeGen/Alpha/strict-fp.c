// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -ffp-exception-behavior=strict \
// RUN:   -emit-llvm -o - %s 2>&1 | FileCheck %s

// The target reports that it supports constrained floating point, so an
// arithmetic operation becomes a constrained intrinsic carrying the rounding
// mode and the exception behaviour rather than a plain fadd.  A target that
// does not report it gets a warning and the ordinary node.

// CHECK-NOT: warning
// CHECK-LABEL: define {{.*}}@add
// CHECK: call double @llvm.experimental.constrained.fadd.f64(
// CHECK-SAME: metadata !"round.tonearest", metadata !"fpexcept.strict")
double add(double a, double b) { return a + b; }
