// Unwind tables are on by default, as they are for this target's gcc, which
// reports -funwind-tables enabled in `gcc -Q --help=common`.  Without them the
// driver leaves the arch at UnwindTableLevel::None, CFI goes to .debug_frame
// and no .eh_frame is emitted: backtrace() cannot walk a C frame and a C++
// exception thrown through one reaches std::terminate.

// RUN: %clang --target=alpha-unknown-linux-gnu -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYNC
// RUN: %clang --target=alpha-unknown-linux-gnu -O2 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SYNC
// SYNC: "-funwind-tables=1"

// Level 1 and not 2: gcc leaves -fasynchronous-unwind-tables off here, so the
// tables are accurate at call sites and not at every instruction.  Asking for
// the asynchronous form still gets it.
// RUN: %clang --target=alpha-unknown-linux-gnu -fasynchronous-unwind-tables \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=ASYNC
// ASYNC: "-funwind-tables=2"

// And -fno-unwind-tables still turns them off.
// RUN: %clang --target=alpha-unknown-linux-gnu -fno-unwind-tables -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=NONE
// NONE-NOT: "-funwind-tables=

// A freestanding compilation opts out too, which is the shape a kernel build
// takes.
// RUN: %clang --target=alpha-unknown-linux-gnu -ffreestanding -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE
