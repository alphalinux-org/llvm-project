// Check that -mbuild-constants maps to the +build-constants target feature.

// RUN: %clang --target=alpha-unknown-linux-gnu -mbuild-constants -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ON
// ON: "-target-feature" "+build-constants"

// RUN: %clang --target=alpha-unknown-linux-gnu -mno-build-constants -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=OFF
// OFF: "-target-feature" "-build-constants"

// RUN: %clang --target=alpha-unknown-linux-gnu -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=DEFAULT
// DEFAULT-NOT: "+build-constants"
// DEFAULT-NOT: "-build-constants"
