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

// 128-bit long double.
// CHECK-DAG: #define __LONG_DOUBLE_128__ 1
// CHECK-DAG: #define __SIZEOF_LONG_DOUBLE__ 16

// -mlong-double-64 makes long double a plain double, so the macro glibc's
// ldbl-opt headers switch on must not be defined.
// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -mlong-double-64 \
// RUN:   < /dev/null | FileCheck %s --check-prefix=LD64 \
// RUN:       --implicit-check-not=__LONG_DOUBLE_128__
// LD64: #define __SIZEOF_LONG_DOUBLE__ 8
