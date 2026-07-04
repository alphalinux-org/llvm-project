// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// The Alpha __builtin_va_list is a { base, offset } record: a pointer to the
// argument save area and an int byte offset into it.  Slot N lives at
// base + N*8 for every N, because the six integer argument registers are saved
// immediately below the incoming stack pointer and the caller's stack
// arguments continue from there.
//
// __offset is an int, matching gcc's alpha_build_builtin_va_list.  It is ABI:
// a gcc va_start writes four bytes here, so reading eight picks up whatever
// the tail padding happens to hold.

// CHECK: %struct.__va_list_tag = type { ptr, i32 }

// An integer argument is read straight out of its slot.
// CHECK-LABEL: define {{.*}}i64 @test_int
// CHECK: %ap.base = load ptr, ptr %ap.base.addr
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: [[EXT:%.*]] = sext i32 %ap.offset to i64
// CHECK: %ap.cur = getelementptr i8, ptr %ap.base, i64 [[EXT]]
// CHECK: %ap.next = add i32 %ap.offset, 8
// CHECK: store i32 %ap.next, ptr %ap.offset.addr
// CHECK: load i64, ptr %ap.cur
long test_int(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  long r = __builtin_va_arg(ap, long);
  __builtin_va_end(ap);
  return r;
}

// A floating-point argument still in one of the six register slots comes from
// the floating-point save area, which sits 48 bytes below the integer one.
// The offset stored back is the unbiased one.
// CHECK-LABEL: define {{.*}}double @test_double
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: %ap.in.regs = icmp ult i32 %ap.offset, 48
// CHECK: [[BIASED:%.*]] = sub i32 %ap.offset, 48
// CHECK: %ap.eff.offset = select i1 %ap.in.regs, i32 [[BIASED]], i32 %ap.offset
// CHECK: [[EXT:%.*]] = sext i32 %ap.eff.offset to i64
// CHECK: %ap.cur = getelementptr i8, ptr %ap.base, i64 [[EXT]]
// CHECK: %ap.next = add i32 %ap.offset, 8
// CHECK: load double, ptr %ap.cur
double test_double(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  double r = __builtin_va_arg(ap, double);
  __builtin_va_end(ap);
  return r;
}

struct S3 { long a, b, c; };
