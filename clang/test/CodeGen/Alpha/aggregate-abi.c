// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// The OSF ABI passes aggregates by value as consecutive 64-bit quadwords in the
// integer registers (then the stack) and returns them in memory via a hidden
// pointer.

struct s1 { long a; };
struct s2 { long a, b; };
struct s3 { long a, b, c; };
struct fp2 { double a, b; };

// An 8-byte record is coerced to a single i64.
// CHECK-LABEL: define {{.*}}i64 @take8(i64 %
long take8(struct s1 x) { return x.a; }

// A 16-byte record is coerced to [2 x i64] and passed by value (not byval/sret).
// CHECK-LABEL: define {{.*}}i64 @take16([2 x i64] %
long take16(struct s2 x) { return x.a + x.b; }

// CHECK-LABEL: define {{.*}}i64 @take24([3 x i64] %
long take24(struct s3 x) { return x.a + x.b + x.c; }

// Floating-point members are still passed in the integer registers.
// CHECK-LABEL: define {{.*}}double @takefp2([2 x i64] %
double takefp2(struct fp2 x) { return x.a + x.b; }

// Aggregates are returned indirectly through an sret pointer.
// CHECK-LABEL: define {{.*}}void @mk2(ptr {{.*}}sret
struct s2 mk2(long a, long b) { struct s2 r = {a, b}; return r; }
