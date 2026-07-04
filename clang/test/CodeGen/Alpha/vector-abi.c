// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// GCC's vector extension types follow the same rules as everything else on
// this ABI, which is to say the integer ones.  alpha_function_arg gives an
// argument the floating-point registers only when its mode class is
// MODE_FLOAT; a vector's is MODE_VECTOR_INT or MODE_VECTOR_FLOAT, so it goes
// in the integer registers, packed, taking ALPHA_ARG_SIZE quadwords.
// alpha_return_in_memory sends every MODE_VECTOR_FLOAT to memory "like an
// aggregate" and an integer vector once it is wider than a register.
//
// Without a rule of its own a vector reaches a back end with no legal vector
// type and is scalarised one register per element, which shifts every argument
// after it and returns a float vector in $f0/$f1 where the caller passed a
// hidden pointer.

typedef int    V2SI __attribute__((vector_size(8)));
typedef float  V2SF __attribute__((vector_size(8)));
typedef char   V8QI __attribute__((vector_size(8)));
typedef int    V4SI __attribute__((vector_size(16)));
typedef float  V4SF __attribute__((vector_size(16)));

// An 8-byte vector is one quadword, whatever its element type, and the
// arguments around it keep their own slots.
// CHECK-LABEL: define {{.*}}i64 @take_v2si(i64 {{.*}}%{{.*}}, i64 {{.*}}%v.coerce, i64 {{.*}}%
long take_v2si(long p, V2SI v, long q) { return p + v[0] + v[1] + q; }

// CHECK-LABEL: define {{.*}}i64 @take_v2sf(i64 {{.*}}%{{.*}}, i64 {{.*}}%v.coerce, i64 {{.*}}%
long take_v2sf(long p, V2SF v, long q) { return p + (long)v[0] + q; }

// CHECK-LABEL: define {{.*}}i64 @take_v8qi(i64 {{.*}}%{{.*}}, i64 {{.*}}%v.coerce, i64 {{.*}}%
long take_v8qi(long p, V8QI v, long q) { return p + v[0] + q; }

// A 16-byte one is two, coerced the way a 16-byte record is.
// CHECK-LABEL: define {{.*}}i64 @take_v4si(i64 {{.*}}%{{.*}}, [2 x i64] {{.*}}%v.coerce, i64 {{.*}}%
long take_v4si(long p, V4SI v, long q) { return p + v[0] + v[3] + q; }

// An integer vector that fits a register comes back in $0.
// CHECK-LABEL: define {{.*}}i64 @ret_v2si(
V2SI ret_v2si(int x) { V2SI v = {x, x + 1}; return v; }

// CHECK-LABEL: define {{.*}}i64 @ret_v8qi(
V8QI ret_v8qi(char x) { V8QI v = {x, 1, 2, 3, 4, 5, 6, 7}; return v; }

// A float vector goes to memory however narrow it is.
// CHECK-LABEL: define {{.*}}void @ret_v2sf(ptr {{.*}}sret(<2 x float>)
V2SF ret_v2sf(float x) { V2SF v = {x, x}; return v; }

// CHECK-LABEL: define {{.*}}void @ret_v4sf(ptr {{.*}}sret(<4 x float>)
V4SF ret_v4sf(float x) { V4SF v = {x, x, x, x}; return v; }

// And so does an integer vector wider than a register.
// CHECK-LABEL: define {{.*}}void @ret_v4si(ptr {{.*}}sret(<4 x i32>)
V4SI ret_v4si(int x) { V4SI v = {x, x, x, x}; return v; }
