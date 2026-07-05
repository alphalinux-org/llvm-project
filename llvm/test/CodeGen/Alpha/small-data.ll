; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data < %s | FileCheck %s

; With -msmall-data, a small locally-defined global is placed in .sdata/.sbss
; and addressed relative to the global pointer; large, external, or explicitly
; sectioned globals stay in the GOT.
;
; Placement and addressing are separate questions.  A preemptible definition is
; still small enough for .sdata, but its address is whatever the dynamic linker
; picks, so it has to come from the GOT.  gcc does the same: built
; -fPIC -msmall-data, a default-visibility global lands in .sbss and is still
; loaded with !literal.

@small = dso_local global i64 7
@smallbss = dso_local global i64 0
@big = dso_local global [64 x i64] zeroinitializer
@ext = external global i64
@preempt = global i64 7

; CHECK-LABEL: get_small:
; CHECK: ldah $0, small($29) !gprelhigh
; CHECK: lda $0, small($0) !gprellow
define ptr @get_small() {
  ret ptr @small
}

; Size decides placement, not addressing: @big is too large for .sdata but is
; still a fixed distance from gp, so its address is built the same way.
; CHECK-LABEL: get_big:
; CHECK: ldah $0, big($29) !gprelhigh
; CHECK: lda $0, big($0) !gprellow
define ptr @get_big() {
  ret ptr @big
}

; CHECK-LABEL: get_ext:
; CHECK: ldq $0, ext($29) !literal
define ptr @get_ext() {
  ret ptr @ext
}

; Small enough for .sdata, but preemptible, so the address comes from the GOT.
; CHECK-LABEL: get_preempt:
; CHECK: ldq $0, preempt($29) !literal
; CHECK-NOT: gprelhigh
define ptr @get_preempt() {
  ret ptr @preempt
}

; CHECK: .section .sdata,"aw",@progbits
; CHECK: small:
; CHECK: .section .sbss,"aw",@nobits
; CHECK: smallbss:
; @big stays in a regular data section, not .sdata.
; CHECK: .section .bss
; CHECK: big:
; Being preemptible does not keep a small global out of .sdata; it only decides
; how its address is formed.
; CHECK: .section .sdata,"aw",@progbits
; CHECK: preempt:
