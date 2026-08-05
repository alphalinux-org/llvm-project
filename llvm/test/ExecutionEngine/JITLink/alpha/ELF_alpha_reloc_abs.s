# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t.o %s
# RUN: llvm-jitlink -noexec \
# RUN:     -abs X=0x0123456789abcdef -abs Y=0x12345678 \
# RUN:     -check=%s %t.o
#
# Check R_ALPHA_REFQUAD and R_ALPHA_REFLONG.

# jitlink-check: *{8}Q = X
# jitlink-check: *{4}L = Y

        .text
        .globl  main
        .type   main,@function
main:
        ret
        .size   main, .-main

        .data
        .globl  Q
        .p2align 3
Q:
        .quad   X
        .size   Q, 8

        .globl  L
        .p2align 2
L:
        .long   Y
        .size   L, 4
