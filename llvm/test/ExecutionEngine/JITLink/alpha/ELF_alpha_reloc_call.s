# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_reloc.o %s
# RUN: llvm-jitlink -noexec -check=%s %t/elf_reloc.o
#
# Check the relocations a call carries: the ldgp pair (R_ALPHA_GPDISP), the
# procedure value loaded out of the GOT (R_ALPHA_LITERAL), a direct branch
# (R_ALPHA_BRADDR) and the branch-prediction hint on an indirect one
# (R_ALPHA_HINT).

# The global pointer addresses a 64k window centered on the GOT, so the first
# entry sits 0x8000 below it and the ldq that reads it holds that displacement.
# jitlink-check: *{2}(main+8) = 0x8000

# R_ALPHA_GPDISP is one relocation covering both halves of the ldgp pair: $27
# holds the entry point, so the displacement written across them is from there
# to the global pointer, with the low half signed and its sign carried into the
# high half.
# jitlink-check: (*{2}(main+0))[15:0] = \
# jitlink-check:   ((((section_addr(elf_reloc.o, $__GOT) + 0x8000) - main) \
# jitlink-check:     + 0x8000) >> 16) & 0xffff
# jitlink-check: (*{2}(main+4))[15:0] = \
# jitlink-check:   ((section_addr(elf_reloc.o, $__GOT) + 0x8000) - main) & 0xffff

# The hint is where the jsr is predicted to land, counted like a branch from
# the instruction after it.  Only 14 bits of it are kept: it steers the return
# stack and nothing reads it for correctness, so a target too far away is
# truncated rather than rejected.
# jitlink-check: (*{4}(main+12))[13:0] = \
# jitlink-check:   ((callee - (main + 16)) >> 2)[13:0]

# The branch counts instructions from the one after it, and the only
# instruction in between is the ret, so the displacement is 1.
# jitlink-check: (*{4}(main+16))[20:0] = 1

        .text
        .globl  main
        .type   main,@function
main:
        ldgp $29, 0($27)
        ldq  $27, callee($29)   !literal
        jsr  $26, ($27), callee
        bsr  $26, target
        ret
        .size   main, .-main

        .globl  target
        .type   target,@function
target:
        ret
        .size   target, .-target

        .globl  callee
        .type   callee,@function
callee:
        ret
        .size   callee, .-callee
