// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// gcc's TARGET_SPLIT_COMPLEX_ARG splits every complex actual into its two
// parts before any per-argument ABI classification runs, so on alpha an
// unnamed _Complex float becomes two SFmode arguments -- and an unnamed SFmode
// is passed by reference (alpha_pass_by_reference), so the pair occupies two
// argument slots holding two pointers to two caller-made copies.  No
// ABIArgInfo kind describes that, so the split is performed on the argument
// list itself, at the call site, and va_arg reads the two slots back.
//
// Getting this wrong is silent: one pointer to the pair in one slot leaves
// every following argument a slot early, and a gcc callee dereferences a
// second pointer that was never supplied.

_Complex float cf(void);
void v(int k, ...);

// Two byval floats in two slots, and the argument after them keeps its place.
// CHECK-LABEL: define {{.*}}void @call(
// CHECK:      %[[R:.*]] = extractvalue { float, float } %{{.*}}, 0
// CHECK-NEXT: %[[I:.*]] = extractvalue { float, float } %{{.*}}, 1
// CHECK-NEXT: store float %[[R]], ptr %[[RT:[^,]*]], align 4
// CHECK-NEXT: store float %[[I]], ptr %[[IT:[^,]*]], align 4
// CHECK-NEXT: call void (i32, ...) @v(i32 noundef signext 1,
// CHECK-SAME:   ptr noundef byval(float) align 4 %[[RT]],
// CHECK-SAME:   ptr noundef byval(float) align 4 %[[IT]],
// CHECK-SAME:   i64 noundef 7)
// CHECK-NOT:  byval
void call(void) { v(1, cf(), 7L); }

// The same for a literal, and for two of them in a row.
// CHECK-LABEL: define {{.*}}void @call2(
// CHECK: call void (i32, ...) @v(i32 noundef signext 1,
// CHECK-SAME: ptr noundef byval(float) align 4 %{{[^,]*}},
// CHECK-SAME: ptr noundef byval(float) align 4 %{{[^,]*}},
// CHECK-SAME: ptr noundef byval(float) align 4 %{{[^,]*}},
// CHECK-SAME: ptr noundef byval(float) align 4 %{{[^,]*}},
// CHECK-SAME: i64 noundef 9)
void call2(void) { v(1, 5.0f + 6.0fi, 2.5f - 3.25fi, 9L); }

// _Complex double is split into two halves that go in registers directly, not
// by reference; it is unaffected.
// CHECK-LABEL: define {{.*}}void @call_double(
// CHECK-NOT: byval
// CHECK: call void (i32, ...) @v(i32 noundef signext 1, double noundef %{{[^,]*}},
// CHECK-SAME: double noundef %{{[^,]*}}, i64 noundef 7)
void call_double(void) { v(1, 5.0 + 6.0i, 7L); }

// A named _Complex float parameter is classified with the rest of the
// prototype and is passed in a floating-point register pair like any other
// complex: the split applies to unnamed arguments only.
// CHECK-LABEL: define {{.*}}float @named(float noundef %z.coerce0, float noundef %z.coerce1)
float named(_Complex float z) { return __real__ z; }

typedef __builtin_va_list va_list;
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end

// Reading one back takes two slots, each holding a pointer that has to be
// loaded before the float is.  Neither slot is biased into the floating-point
// half of the save area: what is passed is a pointer, and pointers go in the
// integer registers.
// CHECK-LABEL: define {{.*}}float @rd(
// CHECK:      %[[O1:.*]] = load i32, ptr %ap.offset.addr
// CHECK-NOT:  icmp ult
// CHECK:      %ap.cur = getelementptr i8, ptr %ap.base, i64
// CHECK:      add i32 %[[O1]], 8
// CHECK:      %ap.indirect = load ptr, ptr %ap.cur
// CHECK-NEXT: %ap.real = load float, ptr %ap.indirect
// CHECK:      %[[O2:.*]] = load i32, ptr %ap.offset.addr
// CHECK-NOT:  icmp ult
// CHECK:      add i32 %[[O2]], 8
// CHECK:      %[[P2:.*]] = load ptr, ptr %ap.cur
// CHECK-NEXT: %ap.imag = load float, ptr %[[P2]]
float rd(int k, ...) {
  va_list ap;
  va_start(ap, k);
  _Complex float z = va_arg(ap, _Complex float);
  va_end(ap);
  return __real__ z;
}
