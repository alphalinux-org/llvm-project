// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -emit-llvm -o - %s | FileCheck %s

// __builtin_va_list is an ABI type, so its layout has to match gcc's
// alpha_build_builtin_va_list exactly.  Measured with gcc 16.2 on alpha:
//
//   sizeof=16 align=8 offset_off=8 offset_sz=4
//
// The total size and alignment come out the same whether __offset is int or
// long, which is why passing a clang-built va_list to glibc happens to work
// either way.  The width is what differs: a gcc va_start writes four bytes at
// offset 8, so a consumer that reads eight picks up the tail padding with it.

typedef __builtin_va_list va_list;

// CHECK: %struct.__va_list_tag = type { ptr, i32 }

// sizeof, alignof, the offset of __offset, and its width.
// CHECK: @layout = global [4 x i64] [i64 16, i64 8, i64 8, i64 4]
unsigned long layout[4] = {sizeof(va_list), __alignof__(va_list),
                           __builtin_offsetof(va_list, __offset),
                           sizeof(((va_list *)0)->__offset)};

va_list g;

// A va_arg reads slot N at base + N*8 and moves the offset on by one slot.
// For a floating type the slot is in the floating half of the save area, which
// sits 48 bytes below the base -- but only while the offset is still inside
// the register save area; past it the arguments were passed on the stack and
// there is one copy, not two.
//
// _Complex float is not here: gcc splits one into two by-reference halves in
// two slots, so both of its slots hold pointers rather than values.  See
// CodeGen/Alpha/complex-float-vararg.c.
//
// Every other _Complex is split into its two parts, each taking a slot of its
// own, so each part is fetched the way a scalar of the element type would be.
// An integer part comes from the integer save area with no bias; a floating
// part takes the same -48 bias a scalar double does.
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_end __builtin_va_end

// CHECK-LABEL: define dso_local signext i32 @read(i32 noundef signext %n, ...)
int read(int n, ...) {
  va_list ap;
  va_start(ap, n);
  // CHECK:      %[[OFF:.*]] = load i32, ptr %ap.offset.addr
  // CHECK:      getelementptr i8, ptr %ap.base, i64
  // CHECK:      add i32 %[[OFF]], 8
  int i = va_arg(ap, int);
  // CHECK:      %[[REG:.*]] = icmp ult i32 %[[OFF2:.*]], 48
  // CHECK-NEXT: %[[ADJ:.*]] = sub i32 %[[OFF2]], 48
  // CHECK-NEXT: select i1 %[[REG]], i32 %[[ADJ]], i32 %[[OFF2]]
  double d = va_arg(ap, double);
  // The two integer parts are two unbiased slots, one after the other: no
  // save-area bias is applied to either.
  // CHECK:      %[[CI1:.*]] = load i32, ptr %ap.offset.addr
  // CHECK-NOT:  icmp ult
  // CHECK:      %[[CIR:.*]] = getelementptr i8, ptr %{{.*}}, i64
  // CHECK:      add i32 %[[CI1]], 8
  // CHECK:      %ap.real = load i32, ptr %[[CIR]]
  // CHECK-NOT:  icmp ult
  // CHECK:      %[[CII:.*]] = getelementptr i8, ptr %{{.*}}, i64
  // CHECK:      %ap.imag = load i32, ptr %[[CII]]
  _Complex int ci = va_arg(ap, _Complex int);
  // A _Complex double is two slots too, but each takes the same -48 bias a
  // scalar double does.
  // CHECK:      icmp ult i32 %{{.*}}, 48
  // CHECK:      %[[CDR:.*]] = load double, ptr
  // CHECK:      icmp ult i32 %{{.*}}, 48
  // CHECK:      %[[CDI:.*]] = load double, ptr
  _Complex double cd = va_arg(ap, _Complex double);
  va_end(ap);
  return i + (int)d + __real__ ci + (int)__real__ cd;
}
