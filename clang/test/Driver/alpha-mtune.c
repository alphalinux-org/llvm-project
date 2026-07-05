// RUN: %clang -target alpha-linux-gnu -mtune=ev6 -S -### %s 2>&1 | FileCheck %s
// RUN: %clang -target alpha-linux-gnu -mcpu=ev4 -mtune=ev67 -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SPLIT

// CHECK: "-tune-cpu" "ev6"

// SPLIT: "-target-cpu" "ev4"
// SPLIT: "-tune-cpu" "ev67"
