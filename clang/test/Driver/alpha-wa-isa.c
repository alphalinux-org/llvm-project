// An -Wa,-m<cpu> assembler ISA flag selects the extensions GNU as would for the
// same name.  The names and what each permits are its cpu_types table in
// gas/config/tc-alpha.c, which the command line reads (unlike .arch, which
// takes a symbol name and so cannot spell a chip number).
//
// One bit does not line up: GNU as has a single CIX bit gating ctpop/ctlz/cttz
// and itoft/ftoit/sqrtt alike, where we split those into +cix and +fix, so a
// name granting CIX there grants both here.  Its MAX bit is our MVI.

// RUN: %clang -target alpha-linux-gnu -Wa,-mev4 -c -### %s 2>&1 | FileCheck %s --check-prefix=BASE
// RUN: %clang -target alpha-linux-gnu -Wa,-m21064 -c -### %s 2>&1 | FileCheck %s --check-prefix=BASE
// RUN: %clang -target alpha-linux-gnu -Wa,-mall -c -### %s 2>&1 | FileCheck %s --check-prefix=BASE
// BASE-NOT: "+bwx"
// BASE-NOT: "+mvi"

// RUN: %clang -target alpha-linux-gnu -Wa,-mev56 -c -### %s 2>&1 | FileCheck %s --check-prefix=BWX
// RUN: %clang -target alpha-linux-gnu -Wa,-m21164a -c -### %s 2>&1 | FileCheck %s --check-prefix=BWX
// BWX: "+bwx"
// BWX-NOT: "+mvi"

// pca56 is the 21164PC, which has MVI.  This one was mapped to +bwx alone.
// RUN: %clang -target alpha-linux-gnu -Wa,-mpca56 -c -### %s 2>&1 | FileCheck %s --check-prefix=MVI
// RUN: %clang -target alpha-linux-gnu -Wa,-m21164pc -c -### %s 2>&1 | FileCheck %s --check-prefix=MVI
// MVI-DAG: "+bwx"
// MVI-DAG: "+mvi"
// MVI-NOT: "+cix"

// RUN: %clang -target alpha-linux-gnu -Wa,-mev6 -c -### %s 2>&1 | FileCheck %s --check-prefix=CIX
// RUN: %clang -target alpha-linux-gnu -Wa,-mev67 -c -### %s 2>&1 | FileCheck %s --check-prefix=CIX
// RUN: %clang -target alpha-linux-gnu -Wa,-m21264 -c -### %s 2>&1 | FileCheck %s --check-prefix=CIX
// CIX-DAG: "+bwx"
// CIX-DAG: "+mvi"
// CIX-DAG: "+fix"
// CIX-DAG: "+cix"
