// The CIX and MVI builtins require the cix and mvi target features, which the
// driver derives from -mcpu or the -mcix/-mmax flags.
// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -target-feature +cix \
// RUN:   -target-feature +mvi -emit-llvm -O0 -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -verify -emit-llvm -o - %s

// Without the feature, using a gated builtin is an error.  Code generation
// stops at the first such builtin, so only one error is checked here.
long test_gate(long x) {
  return __builtin_alpha_ctpop(x); // expected-error {{needs target feature cix}}
}

// CHECK-LABEL: @test_ctpop
// CHECK: call i64 @llvm.alpha.ctpop(i64 %{{.*}})
long test_ctpop(long x) { return __builtin_alpha_ctpop(x); }

// CHECK-LABEL: @test_ctlz
// CHECK: call i64 @llvm.alpha.ctlz(i64 %{{.*}})
long test_ctlz(long x) { return __builtin_alpha_ctlz(x); }

// CHECK-LABEL: @test_minub8
// CHECK: call i64 @llvm.alpha.minub8(i64 %{{.*}}, i64 %{{.*}})
long test_minub8(long x, long y) { return __builtin_alpha_minub8(x, y); }

// CHECK-LABEL: @test_perr
// CHECK: call i64 @llvm.alpha.perr(i64 %{{.*}}, i64 %{{.*}})
long test_perr(long x, long y) { return __builtin_alpha_perr(x, y); }

// CHECK-LABEL: @test_unpkbw
// CHECK: call i64 @llvm.alpha.unpkbw(i64 %{{.*}})
long test_unpkbw(long x) { return __builtin_alpha_unpkbw(x); }
