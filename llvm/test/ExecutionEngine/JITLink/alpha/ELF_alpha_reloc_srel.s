# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_reloc.o %s
# RUN: llvm-jitlink -noexec -check=%s %t/elf_reloc.o
#
# R_ALPHA_SREL32 and R_ALPHA_SREL64 hold the distance from the datum to its
# target, which is how a position-independent table of addresses is built.  The
# JITLink edges they become are Delta32 and Delta64.

        .text
        .globl  main
        .type   main,@function
main:
        ret
        .size   main, .-main

        .globl  target
        .type   target,@function
target:
        ret
        .size   target, .-target

        .data
        .p2align 3
# The value is target's address minus this word's own address.
# jitlink-check: *{4}(delta32) = target - delta32
        .globl  delta32
delta32:
        .long   target - .

# jitlink-check: *{8}(delta64) = target - delta64
        .p2align 3
        .globl  delta64
delta64:
        .quad   target - .

# A difference taken the other way round is negative, which is the case a
# subtraction with the wrong base would get wrong while still looking plausible.
# jitlink-check: *{4}(back32) = main - back32
        .p2align 3
        .globl  back32
back32:
        .long   main - .
