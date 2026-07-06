// RUN: %clang --target=alpha-unknown-linux-gnu -mtune=ev6 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=MTUNE
// MTUNE: "-tune-cpu" "ev6"

// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLDATA
// SMALLDATA: "-target-feature" "+small-data"

// RUN: %clang --target=alpha-unknown-linux-gnu -mlarge-data \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGEDATA
// LARGEDATA-NOT: "+small-data"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=su \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPTRAP_SU
// FPTRAP_SU: "-target-feature" "+ieee"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=sui \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPTRAP_SUI
// FPTRAP_SUI: "-target-feature" "+ieee-with-inexact"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=u \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPTRAP_U
// FPTRAP_U: "-target-feature" "+fptrap-u"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=d \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPROUND_D
// FPROUND_D: "-target-feature" "+fpround-dynamic"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=m \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPROUND_M
// FPROUND_M: "-target-feature" "+fpround-minus"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=c \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=FPROUND_C
// FPROUND_C: "-target-feature" "+fpround-chopped"

// RUN: %clang --target=alpha-unknown-linux-gnu -mtrap-precision=i \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=TRAPPRECISION_I
// TRAPPRECISION_I: "-target-feature" "+trap-precision-insn"

// -mieee-conformant marks the object and implies nothing else; gcc documents
// a `.eflag 48' in each function prologue as its whole effect.  The trapping
// and precision modes are the user's to ask for, which is why they are passed
// here as well.  See alpha-ieee.c for the diagnostic when they are not.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-conformant \
// RUN:   -mtrap-precision=i -mfp-trap-mode=su \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=IEEECONFORMANT
// IEEECONFORMANT: "-target-feature" "+ieee-conformant"

// RUN: %clang --target=alpha-unknown-linux-gnu -msmall-text \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SMALLTEXT
// SMALLTEXT: "-target-feature" "+small-text"

// RUN: %clang --target=alpha-unknown-linux-gnu -mlarge-text \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LARGETEXT
// LARGETEXT-NOT: "+small-text"

// RUN: %clang --target=alpha-unknown-linux-gnu -msafe-partial \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=SAFEPARTIAL
// SAFEPARTIAL: "-target-feature" "+safe-partial"

// -mlong-double-128 restates the format long double already has, so the driver
// accepts it and passes nothing down.
// RUN: %clang --target=alpha-unknown-linux-gnu -mlong-double-128 \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=LONGDOUBLE128
// LONGDOUBLE128-NOT: error:
// LONGDOUBLE128: "-cc1"
// LONGDOUBLE128-NOT: "-mlong-double-128"
// LONGDOUBLE128-NOT: error:

int main(void) { return 0; }
