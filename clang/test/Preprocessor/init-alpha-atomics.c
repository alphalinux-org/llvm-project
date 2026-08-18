// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -fgnuc-version=4.2.1 -dM -E %s \
// RUN:   | FileCheck %s

// A 64-bit load or store is a single instruction and ldq_l/stq_c give the
// read-modify-write, so every integer width up to long long, and a pointer,
// are lock-free.

// CHECK-DAG: #define __CLANG_ATOMIC_LLONG_LOCK_FREE 2
// CHECK-DAG: #define __CLANG_ATOMIC_POINTER_LOCK_FREE 2
// CHECK-DAG: #define __GCC_ATOMIC_LLONG_LOCK_FREE 2
// CHECK-DAG: #define __GCC_ATOMIC_POINTER_LOCK_FREE 2

// The sub-word compare-and-swap is expanded to a masked longword loop, so the
// __sync builtins are inlined at every size, as they are with GCC.

// CHECK-DAG: #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1 1
// CHECK-DAG: #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2 1
// CHECK-DAG: #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4 1
// CHECK-DAG: #define __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 1
