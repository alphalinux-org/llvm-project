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
// A prefix made only of -NOT lines cannot fail: it is satisfied by output that
// never reached the compiler at all.  Anchor it on the cc1 invocation first.
// BASE: "-cc1"
// BASE-NOT: "+bwx"
// BASE-NOT: "+mvi"
// BASE-NOT: "+cix"
// BASE-NOT: "+fix"

// RUN: %clang -target alpha-linux-gnu -Wa,-mev56 -c -### %s 2>&1 | FileCheck %s --check-prefix=BWX
// RUN: %clang -target alpha-linux-gnu -Wa,-m21164a -c -### %s 2>&1 | FileCheck %s --check-prefix=BWX
// BWX: "+bwx"
// BWX-NOT: "+mvi"

// pca56 is the 21164PC, which has MVI as well as BWX.
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

// A -Wa,-m argument that is not an ISA name falls through to the generic
// handling, which must still see what the user wrote: the diagnostic names
// "-mnosuchcpu" and "-msoft-float", not the names with the -m stripped off.
// RUN: not %clang -target alpha-linux-gnu -Wa,-mnosuchcpu -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=UNKNOWN
// UNKNOWN: unsupported argument '-mnosuchcpu' to option '-Wa,'

// RUN: not %clang -target alpha-linux-gnu -Wa,-msoft-float -c %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SOFTFLOAT
// SOFTFLOAT: unsupported argument '-msoft-float' to option '-Wa,'
