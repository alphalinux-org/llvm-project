// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s \
// RUN:   | FileCheck %s

// The Alpha ABI keeps a sub-64-bit integer extended to the full width of its
// register.  A 32-bit value is sign-extended even when its type is unsigned;
// anything narrower is extended according to the signedness of its type.  This
// matches GCC's alpha_promote_function_mode.

int max_i(int a, int b) { return a > b ? a : b; }
// CHECK-LABEL: define dso_local signext i32 @max_i(i32 noundef signext {{.*}}, i32 noundef signext {{.*}})

unsigned max_u(unsigned a, unsigned b) { return a > b ? a : b; }
// CHECK-LABEL: define dso_local signext i32 @max_u(i32 noundef signext {{.*}}, i32 noundef signext {{.*}})

short narrow(short x) { return x; }
// CHECK-LABEL: define dso_local signext i16 @narrow(i16 noundef signext {{.*}})

unsigned short narrow_u(unsigned short x) { return x; }
// CHECK-LABEL: define dso_local zeroext i16 @narrow_u(i16 noundef zeroext {{.*}})

signed char sbyte_id(signed char x) { return x; }
// CHECK-LABEL: define dso_local signext i8 @sbyte_id(i8 noundef signext {{.*}})

unsigned char byte_id(unsigned char x) { return x; }
// CHECK-LABEL: define dso_local zeroext i8 @byte_id(i8 noundef zeroext {{.*}})

// A _Bool must be zero-extended: sign-extending it would make `true` all-ones
// rather than 1, which no other compiler on the platform expects.
_Bool eq(int a, int b) { return a == b; }
// CHECK-LABEL: define dso_local zeroext i1 @eq(i32 noundef signext {{.*}}, i32 noundef signext {{.*}})

_Bool not(_Bool x) { return !x; }
// CHECK-LABEL: define dso_local zeroext i1 @not(i1 noundef zeroext {{.*}})

// An enum follows its underlying type: this one is unsigned int, so 32 bits
// wide and therefore still sign-extended.
enum E { E0, E1 };
enum E enum_id(enum E x) { return x; }
// CHECK-LABEL: define dso_local signext i32 @enum_id(i32 noundef signext {{.*}})

long wide(long x) { return x; }
// CHECK-LABEL: define dso_local i64 @wide(i64 noundef {{[^,)]*}})

// A variadic argument is promoted the same way a named one is: the default
// argument promotions widen it to int or double first, and the ABI's extension
// then applies to what is left.  Nothing about being unnamed changes it.
int vf(int n, ...);
int call_variadic(short s, unsigned short us, unsigned char c, float f) {
  return vf(1, s, us, c, f);
}
// CHECK-LABEL: define dso_local signext i32 @call_variadic(
// CHECK:       call signext i32 (i32, ...) @vf(i32 noundef signext 1,
// CHECK-SAME:      i32 noundef signext {{%[^,]*}}, i32 noundef signext {{%[^,]*}},
// CHECK-SAME:      i32 noundef signext {{%[^,]*}}, double noundef {{%[^)]*}})

// A variadic function's own parameters are declared the same way.
int variadic(int n, ...) { return n; }
// CHECK-LABEL: define dso_local signext i32 @variadic(i32 noundef signext {{.*}}, ...)
