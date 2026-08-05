# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_brsgp.o %s
# RUN: llvm-jitlink -noexec -check=%s %t/elf_brsgp.o
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj --defsym NOPRO=1 \
# RUN:   -o %t/elf_nopro.o %s
# RUN: not llvm-jitlink -noexec %t/elf_nopro.o 2>&1 | FileCheck %s
#
# R_ALPHA_BRSGP is a `bsr !samegp` to a routine that shares the caller's gp.
# The branch leaves $27 unset, so where it lands depends on how the callee
# establishes gp -- which the callee advertises in st_other.  It is not
# R_ALPHA_BRADDR with a different name: getting the two the same would run a
# standard callee's ldgp pair with a garbage procedure value.

# .prologue 1 (STO_ALPHA_STD_GPLOAD): the callee opens with the two-word gp
# load, which a caller already holding gp must skip.  The branch aims eight
# bytes past the entry point.
# jitlink-check: (*{4}(main + 0))[20:0] = \
# jitlink-check:   (((std_gpload + 8) - (main + 4)) >> 2)[20:0]

# .prologue 0 (STO_ALPHA_NOPV): the callee runs on the caller's gp and has no
# prologue to skip, so the branch aims at the entry point itself.
# jitlink-check: (*{4}(main + 4))[20:0] = \
# jitlink-check:   ((nopv - (main + 8)) >> 2)[20:0]

        .text
        .globl  main
        .type   main,@function
main:
.ifdef NOPRO
        bsr  $26, no_prologue   !samegp
.else
        bsr  $26, std_gpload    !samegp
        bsr  $26, nopv          !samegp
.endif
        ret
        .size   main, .-main

.ifdef NOPRO
# A symbol with neither bit set never said how it establishes gp.  Branching
# past a prologue it did not announce, or into the middle of one, are both
# guesses, so the link fails instead -- as it does under bfd and lld.
# CHECK: !samegp relocation against symbol without .prologue: no_prologue
        .globl  no_prologue
        .type   no_prologue,@function
no_prologue:
        ret
        .size   no_prologue, .-no_prologue
.else
        .globl  std_gpload
        .ent    std_gpload
std_gpload:
        .prologue 1
        ldgp $29, 0($27)
        ret
        .end    std_gpload

        .globl  nopv
        .ent    nopv
nopv:
        .prologue 0
        ret
        .end    nopv
.endif
