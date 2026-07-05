// -msmall-data and -mlarge-data are Alpha specific, so using them on another
// target is an error rather than a silently ignored argument.

// RUN: not %clang --target=x86_64-unknown-linux-gnu -msmall-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLDATA-X86
// SMALLDATA-X86: unsupported option '-msmall-data' for target

// RUN: not %clang --target=x86_64-unknown-linux-gnu -mlarge-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGEDATA-X86
// LARGEDATA-X86: unsupported option '-mlarge-data' for target

// On Alpha the flags select the feature the backend addresses globals with;
// checking only the x86 rejection said nothing about what they do where they
// are accepted.

// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-data -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SMALL
// SMALL: "-target-feature" "+small-data"

// RUN: %clang --target=alpha-unknown-linux-gnu -mlarge-data -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LARGE
// LARGE-NOT: "+small-data"

// Last one wins, as for every other -m pair.
// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-data -mlarge-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LASTWINS
// LASTWINS-NOT: "+small-data"
