// RUN: %clang -target alpha-linux-gnu -mfp-trap-mode=sui -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TRAP
// RUN: %clang -target alpha-linux-gnu -mfp-rounding-mode=c -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=ROUND
// RUN: %clang -target alpha-linux-gnu -mtrap-precision=i -S -### %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=PREC

// The trapping and rounding modes become target features, since each spelling
// is a distinct instruction encoding rather than a mode register setting.

// TRAP: "+ieee-with-inexact"
// ROUND: "+fpround-chopped"
// PREC: "+trap-precision-insn"


// The remaining accepted values, including the two that map to no feature at
// all.

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=n -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TRAP-N
// TRAP-N: "-cc1"
// TRAP-N-NOT: "+ieee"
// TRAP-N-NOT: "+fptrap-u"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=u -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TRAP-U
// TRAP-U: "+fptrap-u"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=su -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TRAP-SU
// TRAP-SU: "+ieee"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=n -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=ROUND-N
// ROUND-N: "-cc1"
// ROUND-N-NOT: "+fpround-"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=m -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=ROUND-M
// ROUND-M: "+fpround-minus"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-rounding-mode=d -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=ROUND-D
// ROUND-D: "+fpround-dynamic"

// -mieee, -mieee-with-inexact and -mfp-trap-mode all set the same thing (GCC's
// alpha_fptm), so the last one on the command line wins.

// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -mfp-trap-mode=n -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=LAST-N
// LAST-N: "-cc1"
// LAST-N-NOT: "+ieee"

// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=n -mieee -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=LAST-IEEE
// LAST-IEEE: "+ieee"

// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-with-inexact -mieee -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=LAST-SU \
// RUN:       --implicit-check-not="+ieee-with-inexact"
// LAST-SU: "+ieee"

// Both spellings together still warn about neither.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -mieee-with-inexact -### -c %s \
// RUN:   2>&1 | FileCheck %s --check-prefix=BOTH
// BOTH-NOT: warning:
// BOTH: "+ieee-with-inexact"
