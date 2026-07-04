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

// An aggregate is passed by value as consecutive 8-byte slots, so va_arg reads
// it in place and advances past all of them.  It must not be read as a pointer:
// the caller stores the members themselves into the argument registers.
// CHECK-LABEL: define {{.*}}void @test_struct
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: [[EXT:%.*]] = sext i32 %ap.offset to i64
// CHECK: %ap.cur = getelementptr i8, ptr %ap.base, i64 [[EXT]]
// CHECK-NOT: load ptr, ptr %ap.cur
// CHECK: %ap.next = add i32 %ap.offset, 24
// CHECK: call void @llvm.memcpy{{.*}}(ptr align 8 %agg.result, ptr align 8 %ap.cur, i64 24,
struct S3 test_struct(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  struct S3 r = __builtin_va_arg(ap, struct S3);
  __builtin_va_end(ap);
  return r;
}

// long double is passed by invisible reference, so its slot holds a pointer
// that has to be loaded before the value is.  It occupies one slot, not two.
// CHECK-LABEL: define {{.*}}void @test_long_double
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: [[EXT:%.*]] = sext i32 %ap.offset to i64
// CHECK: %ap.cur = getelementptr i8, ptr %ap.base, i64 [[EXT]]
// CHECK: %ap.next = add i32 %ap.offset, 8
// CHECK: %ap.indirect = load ptr, ptr %ap.cur
// CHECK: load fp128, ptr %ap.indirect
long double test_long_double(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  long double r = __builtin_va_arg(ap, long double);
  __builtin_va_end(ap);
  return r;
}

// A _Complex float or _Complex double is passed as its two parts in two
// consecutive floating-point argument registers, so va_arg has to fetch each
// part the way a scalar of the element type would be fetched -- including the
// bias into the floating-point save area 48 bytes below __base.  Reading the
// pair as one 16-byte object takes it from the integer save area instead, which
// on alpha faults rather than merely returning the wrong number.  gcc's
// alpha_gimplify_va_arg_1 recurses on the element type twice for this reason.
// CHECK-LABEL: define {{.*}}@test_complex
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: %ap.in.regs = icmp ult i32 %ap.offset, 48
// CHECK: %ap.eff.offset = select i1 %ap.in.regs,
// CHECK: %ap.real = load double, ptr %ap.cur
// CHECK: %ap.offset{{[0-9]*}} = load i32, ptr %ap.offset.addr
// CHECK: %ap.in.regs{{[0-9]*}} = icmp ult i32 %ap.offset{{[0-9]*}}, 48
// CHECK: %ap.imag = load double, ptr %ap.cur{{[0-9]*}}
_Complex double test_complex(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  _Complex double r = __builtin_va_arg(ap, _Complex double);
  __builtin_va_end(ap);
  return r;
}

// __int128 is passed as two consecutive slots, like any 16-byte aggregate, so
// va_arg reads it in place and advances the offset by 16.  Nothing covered it:
// every other case here is one slot or an aggregate read with memcpy.
// CHECK-LABEL: define {{.*}}@test_int128
// CHECK: %ap.offset = load i32, ptr %ap.offset.addr
// CHECK: %ap.next = add i32 %ap.offset, 16
__int128 test_int128(int n, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, n);
  __int128 r = __builtin_va_arg(ap, __int128);
  __builtin_va_end(ap);
  return r;
}
