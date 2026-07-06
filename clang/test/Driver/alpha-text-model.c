// RUN: %clang -target alpha-linux-gnu -msmall-text -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SMALL
// RUN: %clang -target alpha-linux-gnu -mlarge-text -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=LARGE

// -msmall-text calls with a single bsr, assuming the whole program is in range.

// SMALL: "+small-text"
// LARGE: "-small-text"
