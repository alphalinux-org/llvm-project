// The frame pointer is omitted once optimization is on, as it is for every
// other 64-bit Linux target.  Alpha has no frame chain -- an unwinder reads
// the frame size out of the procedure descriptor or the CFI, never a saved
// $fp -- so keeping one costs three instructions and a stack slot in every
// function, leaf functions included, and takes $15 out of allocation
// everywhere, for nothing.

// RUN: %clang --target=alpha-unknown-linux-gnu -O2 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OPT
// OPT-NOT: "-mframe-pointer=all"

// RUN: %clang --target=alpha-unknown-linux-gnu -O1 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OPT

// RUN: %clang --target=alpha-unknown-linux-gnu -Os -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OPT

// Unoptimized code keeps it, so a debugger has the frame to walk.
// RUN: %clang --target=alpha-unknown-linux-gnu -O0 -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOOPT
// NOOPT: "-mframe-pointer=all"

// And -fno-omit-frame-pointer still asks for one.
// RUN: %clang --target=alpha-unknown-linux-gnu -O2 -fno-omit-frame-pointer \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=NOOPT
