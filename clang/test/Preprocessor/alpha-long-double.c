// Check that the Alpha target predefines __LONG_DOUBLE_128__.
// RUN: %clang --target=alpha-unknown-linux-gnu -dM -E - < /dev/null \
// RUN:   | FileCheck %s
// CHECK: #define __LONG_DOUBLE_128__ 1
