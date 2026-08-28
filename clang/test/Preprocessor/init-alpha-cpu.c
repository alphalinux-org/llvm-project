// Each -mcpu= implies the instruction set extensions that model provides, and
// defines the same macros GCC does for it.

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev4 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV4 %s
// EV4: #define __alpha_ev4__ 1
// EV4-NOT: #define __alpha_bwx__

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev5 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV5 %s
// EV5: #define __alpha_ev5__ 1
// EV5-NOT: #define __alpha_bwx__

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev56 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV56 %s
// EV56-DAG: #define __alpha_bwx__ 1
// EV56-DAG: #define __alpha_ev5__ 1
// EV56-NOT: #define __alpha_max__

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu pca56 < /dev/null \
// RUN:   | FileCheck --check-prefix=PCA56 %s
// PCA56-DAG: #define __alpha_bwx__ 1
// PCA56-DAG: #define __alpha_ev5__ 1
// PCA56-DAG: #define __alpha_max__ 1
// PCA56-NOT: #define __alpha_fix__

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev6 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV6 %s
// EV6-DAG: #define __alpha_bwx__ 1
// EV6-DAG: #define __alpha_ev6__ 1
// EV6-DAG: #define __alpha_fix__ 1
// EV6-DAG: #define __alpha_max__ 1
// EV6-NOT: #define __alpha_cix__

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev67 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV67 %s
// EV67-DAG: #define __alpha_bwx__ 1
// EV67-DAG: #define __alpha_cix__ 1
// EV67-DAG: #define __alpha_ev6__ 1
// EV67-DAG: #define __alpha_fix__ 1
// EV67-DAG: #define __alpha_max__ 1

// A part number and the EV name of the same chip are the same processor, so
// they define the same macros -- family macro included.  GCC's alpha_cpu_table
// pairs them the same way, and 21164PC is the one name it also accepts in caps.

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21064 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV4 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21164 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV5 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21164a < /dev/null \
// RUN:   | FileCheck --check-prefix=EV56 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21164pc < /dev/null \
// RUN:   | FileCheck --check-prefix=PCA56 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21164PC < /dev/null \
// RUN:   | FileCheck --check-prefix=PCA56 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21264 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV6 %s

// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu 21264a < /dev/null \
// RUN:   | FileCheck --check-prefix=EV67 %s

// ev45 has no part-number alias of its own (GCC spells the 21064A "ev45") and
// shares the EV4 core.
// RUN: %clang_cc1 -E -dM -triple alpha-unknown-linux-gnu -target-cpu ev45 < /dev/null \
// RUN:   | FileCheck --check-prefix=EV4 %s
