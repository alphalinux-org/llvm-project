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
