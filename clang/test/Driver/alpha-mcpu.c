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

// GCC's -mcpu= table spells most processors as a part number as well as an EV
// name, and build systems that predate the EV names use them.  Each numeric
// spelling has to reach the same ProcessorModel, so it implies the same
// extensions and does not draw "not a recognized processor" from the back end.
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21264 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=NUM21264
// NUM21264: "-target-cpu" "21264"
// NUM21264-NOT: not a recognized processor

// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21064 -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21164 -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21164a -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21164pc -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21164PC -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21264a -c -o /dev/null %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=QUIET --allow-empty
// QUIET-NOT: error:
// QUIET-NOT: warning:

// The part numbers carry their EV twin's extensions: 21164a is ev56 (BWX) and
// 21064 is ev4 (none), so only the first can use stb.
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21164a -O2 -S -o - %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=BWX
// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=21064 -O2 -S -o - %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=NOBWX
// BWX: stb
// NOBWX-NOT: stb
void store_byte(char *p, char v) { *p = v; }

// GCC matches these case-sensitively and accepts no other case, so neither
// does this: only 21164PC has a second spelling in GCC's table.
// RUN: not %clang --target=alpha-unknown-linux-gnu -mcpu=21264A -c -o /dev/null \
// RUN:   %s 2>&1 | FileCheck %s --check-prefix=BADCASE
// BADCASE: unknown target CPU '21264A'
