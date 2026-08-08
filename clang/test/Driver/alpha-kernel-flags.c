// Verify the driver translates the kernel's Alpha flags into the right cc1
// options: -mcpu selects the target CPU, -mno-fp-regs and -ffixed-$<n> become
// target features.

// RUN: %clang --target=alpha-unknown-linux-gnu -mcpu=ev6 -mno-fp-regs -ffixed-8 \
// RUN:   -### -c %s 2>&1 | FileCheck %s

// CHECK: "-target-cpu" "ev6"
// CHECK: "-target-feature" "+no-fp-regs"
// CHECK: "-target-feature" "+reserve-r8"
