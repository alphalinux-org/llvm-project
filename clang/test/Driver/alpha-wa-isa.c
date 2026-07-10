// RUN: %clang -target alpha-linux-gnu -Wa,-mev6 -c -### %s 2>&1 | FileCheck %s

// An -Wa,-mevN assembler ISA flag selects the same extensions -mcpu= would, so
// hand-written assembly built with it assembles against the right instruction
// set.

// CHECK-DAG: "+bwx"
// CHECK-DAG: "+mvi"
// CHECK-DAG: "+fix"
