// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s
// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s \
// RUN:   -mlong-double-64 | FileCheck %s --check-prefix=LD64

// long double is the 128-bit X_floating format; the Alpha ABI passes and
// returns it by invisible reference (sret / byval) rather than in registers.

long double identity(long double x) { return x; }
// CHECK-LABEL: define dso_local void @identity(
// CHECK-SAME: ptr dead_on_unwind noalias writable sret(fp128)
// CHECK-SAME: ptr noundef byval(fp128)

long double add(long double a, long double b) { return a + b; }
// CHECK-LABEL: define dso_local void @add(
// CHECK-SAME: ptr dead_on_unwind noalias writable sret(fp128)
// CHECK-SAME: ptr noundef byval(fp128)
// CHECK-SAME: ptr noundef byval(fp128)

// _Complex long double (TCmode) goes the same way, matching GCC's
// alpha_pass_by_reference and alpha_return_in_memory.
_Complex long double cadd(_Complex long double a, _Complex long double b) {
  return a + b;
}
// CHECK-LABEL: define dso_local void @cadd(
// CHECK-SAME: ptr dead_on_unwind noalias writable sret({ fp128, fp128 })
// CHECK-SAME: ptr noundef byval({ fp128, fp128 })
// CHECK-SAME: ptr noundef byval({ fp128, fp128 })

// With -mlong-double-64 long double is just double, so it is passed and
// returned in $f16/$f0 like any other double.
// LD64-LABEL: define dso_local double @identity(double noundef {{.*}})
// LD64-LABEL: define dso_local double @add(double noundef {{.*}}, double noundef {{.*}})
