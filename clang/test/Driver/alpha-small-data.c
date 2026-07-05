// -msmall-data and -mlarge-data are Alpha specific, so using them on another
// target is an error rather than a silently ignored argument.

// RUN: not %clang --target=x86_64-unknown-linux-gnu -msmall-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLDATA-X86
// SMALLDATA-X86: unsupported option '-msmall-data' for target

// RUN: not %clang --target=x86_64-unknown-linux-gnu -mlarge-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGEDATA-X86
// LARGEDATA-X86: unsupported option '-mlarge-data' for target
