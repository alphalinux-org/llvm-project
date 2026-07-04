// The -mieee target features define the GCC-compatible IEEE macros.
// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -target-feature +ieee \
// RUN:   -dM -E %s | FileCheck %s --check-prefix=MIEEE
// RUN: %clang_cc1 -triple alpha-unknown-linux-gnu -target-feature +ieee-with-inexact \
// RUN:   -dM -E %s | FileCheck %s --check-prefix=MINEX
// MIEEE: #define _IEEE_FP 1
// MIEEE-NOT: _IEEE_FP_INEXACT
// MINEX-DAG: #define _IEEE_FP 1
// MINEX-DAG: #define _IEEE_FP_INEXACT 1
