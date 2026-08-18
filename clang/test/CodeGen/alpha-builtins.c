// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -O0 -o - %s | FileCheck %s

// CHECK-LABEL: @test_implver
// CHECK: call i64 @llvm.alpha.implver()
long test_implver(void) { return __builtin_alpha_implver(); }

// CHECK-LABEL: @test_rpcc
// CHECK: call i64 @llvm.alpha.rpcc()
long test_rpcc(void) { return __builtin_alpha_rpcc(); }

// CHECK-LABEL: @test_amask
// CHECK: call i64 @llvm.alpha.amask(i64 %{{.*}})
long test_amask(long x) { return __builtin_alpha_amask(x); }

// CHECK-LABEL: @test_cmpbge
// CHECK: call i64 @llvm.alpha.cmpbge(i64 %{{.*}}, i64 %{{.*}})
long test_cmpbge(long x, long y) { return __builtin_alpha_cmpbge(x, y); }

// CHECK-LABEL: @test_umulh
// CHECK: call i64 @llvm.alpha.umulh(i64 %{{.*}}, i64 %{{.*}})
long test_umulh(long x, long y) { return __builtin_alpha_umulh(x, y); }

// CHECK-LABEL: @test_zap
// CHECK: call i64 @llvm.alpha.zap(i64 %{{.*}}, i64 %{{.*}})
long test_zap(long x, long y) { return __builtin_alpha_zap(x, y); }

// CHECK-LABEL: @test_zapnot
// CHECK: call i64 @llvm.alpha.zapnot(i64 %{{.*}}, i64 %{{.*}})
long test_zapnot(long x, long y) { return __builtin_alpha_zapnot(x, y); }

// CHECK-LABEL: @test_extqh
// CHECK: call i64 @llvm.alpha.extqh(i64 %{{.*}}, i64 %{{.*}})
long test_extqh(long x, long y) { return __builtin_alpha_extqh(x, y); }

// CHECK-LABEL: @test_insql
// CHECK: call i64 @llvm.alpha.insql(i64 %{{.*}}, i64 %{{.*}})
long test_insql(long x, long y) { return __builtin_alpha_insql(x, y); }

// CHECK-LABEL: @test_mskwh
// CHECK: call i64 @llvm.alpha.mskwh(i64 %{{.*}}, i64 %{{.*}})
long test_mskwh(long x, long y) { return __builtin_alpha_mskwh(x, y); }

// __builtin_set_thread_pointer keeps GCC's unprefixed spelling but is an Alpha
// target builtin: no other target can lower it, so declaring it generically
// would let it reach a back end that crashes selecting it.
// CHECK-LABEL: @test_set_thread_pointer
// CHECK: call void @llvm.alpha.set.thread.pointer(ptr %{{.*}})
void test_set_thread_pointer(void *p) { __builtin_set_thread_pointer(p); }
