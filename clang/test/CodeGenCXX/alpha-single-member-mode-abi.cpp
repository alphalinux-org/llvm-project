// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// gcc gives a record whose layout is one member the mode of that member, and
// compute_record_mode (gcc/stor-layout.cc) counts a C++ base as a member like
// any other -- so a class that inherits a single `long double' is TFmode and
// goes by invisible reference, exactly as the base alone does.  An empty base
// occupies nothing under the empty base optimization and is walked over; the
// same class as a *member* is one byte and is not.

struct B { long double x; };
struct D : B { };

struct EB { };
struct DF : EB { float f; };
struct DL : EB { long double x; };

// A base carrying the whole record hands its mode on.
// CHECK-LABEL: define dso_local void @_Z7named_d1D(
// CHECK-SAME: ptr noundef byval(%struct.D)
void named_d(D x) { (void)x; }

// CHECK-LABEL: define dso_local void @_Z8named_dl2DL(
// CHECK-SAME: ptr noundef byval(%struct.DL)
void named_dl(DL x) { (void)x; }

void sink(long, ...);

// CHECK-LABEL: define dso_local void @_Z7pass_df2DF(
// CHECK: call void (i64, ...) @_Z4sinklz(i64 noundef 1, ptr noundef byval(%struct.DF)
void pass_df(DF x) { sink(1, x, 9L); }

// An empty class as a member is one byte, so it is a second member and the
// record keeps BLKmode.
struct MF { float f; EB e; };

// CHECK-LABEL: define dso_local void @_Z7pass_mf2MF(
// CHECK: call void (i64, ...) @_Z4sinklz(i64 noundef 1, i64
void pass_mf(MF x) { sink(1, x, 9L); }

// A record with non-trivial special members is passed by reference to the
// object the caller built, whatever mode it inherits: byval would be a bitwise
// copy the callee then destroys, leaving the caller's object destroyed twice
// and the callee's writes lost.  gcc passes the address for the same reason.
struct N {
  long double x;
  N(const N &);
  ~N();
};

// CHECK-LABEL: define dso_local void @_Z7named_n1N(
// CHECK-SAME: ptr noundef align 16
// CHECK-NOT: byval
void named_n(N x) { (void)x; }

// A polymorphic class carries a vptr the walk cannot see, so it inherits
// nothing.
struct P { long double x; virtual void g(); };

// CHECK-LABEL: define dso_local void @_Z7named_p1P(
// CHECK-NOT: byval
void named_p(P x) { (void)x; }
