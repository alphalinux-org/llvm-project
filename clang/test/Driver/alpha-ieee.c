// The driver passes -mieee/-mieee-with-inexact through as target features.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=IEEE
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-with-inexact -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=INEX
// IEEE: "-target-feature" "+ieee"
// INEX: "-target-feature" "+ieee-with-inexact"

// -mieee-conformant only marks the object: gcc documents its whole effect as a
// `.eflag 48' in each function prologue, and alpha.cc does exactly that.  It
// must not imply the trapping and precision modes, or asking for the mark
// would silently change how arithmetic is generated.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-conformant \
// RUN:   -mtrap-precision=i -mfp-trap-mode=su -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CONF
// CONF: "-target-feature" "+ieee-conformant"

// gcc leaves the combination unchecked; saying so is more useful than marking
// an object whose arithmetic does not hold up.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-conformant -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CONF-ALONE
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-conformant \
// RUN:   -mtrap-precision=i -### -c %s 2>&1 | FileCheck %s --check-prefix=CONF-ALONE
// CONF-ALONE: warning: '-mieee-conformant' marks the object IEEE conformant

// Function trap precision is not implemented.  Accepting it silently would
// give program precision, which traps far from the instruction that raised it.
// RUN: not %clang --target=alpha-unknown-linux-gnu -mtrap-precision=f -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=TPF
// TPF: error: unsupported argument 'f' to option '-mtrap-precision='


// The combination gcc documents as required for a conformant object: the mark
// plus instruction-precise traps plus software completion with inexact.  All
// three reach cc1 together.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee-conformant \
// RUN:   -mtrap-precision=i -mfp-trap-mode=sui -### -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CONFORMANT
// CONFORMANT-DAG: "+ieee-conformant"
// CONFORMANT-DAG: "+trap-precision-insn"
// CONFORMANT-DAG: "+ieee-with-inexact"

// The su and sui trap modes hand the trap to a software-completion handler,
// which needs it precise to the faulting instruction; -mtrap-precision=p
// cannot give that, so +trap-precision-insn is implied anyway and the request
// is diagnosed.  gcc warns in the same words from alpha_option_override.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -mtrap-precision=p \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=TPP
// RUN: %clang --target=alpha-unknown-linux-gnu -mfp-trap-mode=sui \
// RUN:   -mtrap-precision=p -### -c %s 2>&1 | FileCheck %s --check-prefix=TPP
// TPP: warning: fp software completion requires -mtrap-precision=i

// A 21264 traps precisely in hardware, so program precision costs nothing
// there and gcc does not warn either.
// RUN: %clang --target=alpha-unknown-linux-gnu -mieee -mtrap-precision=p \
// RUN:   -mcpu=ev6 -### -c %s 2>&1 | FileCheck %s --check-prefix=TPP-EV6
// TPP-EV6-NOT: fp software completion

// Without a software-completion trap mode there is nothing to complete.
// RUN: %clang --target=alpha-unknown-linux-gnu -mtrap-precision=p \
// RUN:   -### -c %s 2>&1 | FileCheck %s --check-prefix=TPP-NOSU
// TPP-NOSU-NOT: fp software completion
