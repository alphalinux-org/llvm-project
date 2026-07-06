// RUN: %clang --target=alpha-unknown-linux-gnu -mtune=ev6 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=MTUNE
// MTUNE: "-tune-cpu" "ev6"

// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLDATA
// SMALLDATA: "-target-feature" "+small-data"

// RUN: %clang --target=alpha-unknown-linux-gnu -mlarge-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGEDATA
// LARGEDATA-NOT: "+small-data"

// The floating-point trapping, rounding and precision flags have tests of
// their own -- alpha-fp-modes.c and alpha-ieee.c -- with the commit that adds
// them.

// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-text \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLTEXT
// SMALLTEXT: "-target-feature" "+small-text"

// RUN: %clang --target=alpha-unknown-linux-gnu -mlarge-text \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGETEXT
// LARGETEXT-NOT: "+small-text"

// RUN: %clang --target=alpha-unknown-linux-gnu -msafe-partial \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SAFEPARTIAL
// SAFEPARTIAL: "-target-feature" "+safe-partial"

// -mlong-double-128 restates the format long double already has, so the driver
// accepts it and passes nothing down.
// RUN: %clang --target=alpha-unknown-linux-gnu -mlong-double-128 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LONGDOUBLE128
// LONGDOUBLE128-NOT: error:
// LONGDOUBLE128: "-cc1"
// LONGDOUBLE128-NOT: "-mlong-double-128"
// LONGDOUBLE128-NOT: error:

int main(void) { return 0; }
