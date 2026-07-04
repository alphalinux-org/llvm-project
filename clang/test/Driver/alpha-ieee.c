// The driver passes -mieee/-mieee-with-inexact through as target features.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=IEEE
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-with-inexact -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INEX
// IEEE: "-target-feature" "+ieee"
// INEX: "-target-feature" "+ieee-with-inexact"
