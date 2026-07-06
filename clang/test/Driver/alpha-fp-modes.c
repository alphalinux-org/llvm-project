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
