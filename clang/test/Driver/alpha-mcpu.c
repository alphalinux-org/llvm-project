// The instruction-set extensions a processor implies come from its
// ProcessorModel in Alpha.td, reached through -target-cpu.  The driver must
// not push them as -target-feature as well: that duplicates the mapping in a
// second place, where it drifts (it omitted prefetch and precise arithmetic
// traps) without changing anything.

// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=ev67 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=EV67
// EV67: "-target-cpu" "ev67"
// EV67-NOT: "-target-feature" "+bwx"
// EV67-NOT: "-target-feature" "+cix"

// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=ev56 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=EV56
// EV56: "-target-cpu" "ev56"
// EV56-NOT: "-target-feature" "+bwx"

// An explicit -m<ext> flag is still forwarded, since it overrides the
// processor default.
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=ev4 -mbwx \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=EXPLICIT
// EXPLICIT: "-target-feature" "+bwx"
