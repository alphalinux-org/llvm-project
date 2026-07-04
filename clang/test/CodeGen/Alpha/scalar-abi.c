// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// GCC's alpha_return_in_memory returns every value wider than a register in
// memory, judging a complex type by the width of one part rather than of the
// pair.  So __int128 and _Complex long come back through a hidden pointer,
// while _Complex float and _Complex double come back in $f0/$f1 and are passed
// in two floating-point argument registers.
//
// A complex integer narrower than a register is judged the same way:
// alpha_split_complex_arg splits every complex type except TCmode, so the two
// parts are passed in two integer argument registers, and the pair comes back
// packed into $0.

// CHECK-LABEL: define{{.*}} void @ret_i128(ptr {{.*}}sret(i128)
__int128 ret_i128(void) { return 1; }

// CHECK-LABEL: define{{.*}} void @ret_complex_long(ptr {{.*}}sret({ i64, i64 })
_Complex long ret_complex_long(void) { return 1; }

// CHECK-LABEL: define{{.*}} void @ret_long_double(ptr {{.*}}sret(fp128)
long double ret_long_double(void) { return 1; }

// CHECK-LABEL: define{{.*}} { double, double } @ret_complex_double()
_Complex double ret_complex_double(void) { return 1; }

// CHECK-LABEL: define{{.*}} { float, float } @ret_complex_float()
_Complex float ret_complex_float(void) { return 1; }

// A 128-bit integer is still passed by value, as two quadwords.
// CHECK-LABEL: define{{.*}} void @take_i128(ptr {{.*}}sret(i128){{.*}}, i128 noundef
__int128 take_i128(__int128 x) { return x; }

// CHECK-LABEL: define{{.*}} double @take_complex_double(double noundef {{[^,]*}}, double noundef
double take_complex_double(_Complex double z) { return __real__ z; }

// CHECK-LABEL: define{{.*}} float @take_complex_float(float noundef {{[^,]*}}, float noundef
float take_complex_float(_Complex float z) { return __real__ z; }

// CHECK-LABEL: define{{.*}} i64 @ret_complex_int()
_Complex int ret_complex_int(void) { return 1; }

// CHECK-LABEL: define{{.*}} i64 @ret_complex_short()
_Complex short ret_complex_short(void) { return 1; }

// CHECK-LABEL: define{{.*}} i64 @ret_complex_char()
_Complex char ret_complex_char(void) { return 1; }

// CHECK-LABEL: define{{.*}} signext i32 @take_complex_int(i32 noundef {{[^,]*}}, i32 noundef
int take_complex_int(_Complex int z) { return __real__ z; }

// CHECK-LABEL: define{{.*}} signext i16 @take_complex_short(i16 noundef {{[^,]*}}, i16 noundef
short take_complex_short(_Complex short z) { return __real__ z; }
