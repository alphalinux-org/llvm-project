// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu < /dev/null | FileCheck %s

// CHECK-DAG: #define __alpha__ 1
// CHECK-DAG: #define __alpha 1
// CHECK-DAG: #define _LP64 1
// CHECK-DAG: #define __LP64__ 1
// CHECK-DAG: #define __ELF__ 1

// LP64: 64-bit long and pointer, 32-bit int.
// CHECK-DAG: #define __SIZEOF_LONG__ 8
// CHECK-DAG: #define __SIZEOF_POINTER__ 8
// CHECK-DAG: #define __SIZEOF_INT__ 4
// CHECK-DAG: #define __LONG_MAX__ 9223372036854775807L

// Little-endian.
// CHECK-DAG: #define __ORDER_LITTLE_ENDIAN__ 1234
// CHECK-DAG: #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
