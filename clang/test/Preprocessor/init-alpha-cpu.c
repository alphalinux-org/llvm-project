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
