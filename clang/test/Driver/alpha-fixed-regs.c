// GCC's decode_reg_name takes either a bare register number or the name from
// REGISTER_NAMES, so both -ffixed-9 and -ffixed-$9 name $9 there.  Accept both
// spellings so either form in a build system keeps working.

// RUN: %clang --target=alpha-unknown-linux-gnu -ffixed-9 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FIXED9
// RUN: %clang --target=alpha-unknown-linux-gnu -ffixed-$9 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FIXED9
// FIXED9: "-target-feature" "+reserve-r9"

// The kernel dedicates $8 to the current-task pointer.
// RUN: %clang --target=alpha-unknown-linux-gnu -ffixed-$8 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FIXED8
// FIXED8: "-target-feature" "+reserve-r8"
