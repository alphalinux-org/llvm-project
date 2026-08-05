# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t.o %s
# RUN: llvm-jitlink -noexec -check=%s %t.o
#
# Check the relocations a call carries: the ldgp pair (R_ALPHA_GPDISP), the
# procedure value loaded out of the GOT (R_ALPHA_LITERAL) and a direct branch
# (R_ALPHA_BRADDR).

# The global pointer addresses a 64k window centered on the GOT, so the first
# entry sits 0x8000 below it and the ldq that reads it holds that displacement.
# jitlink-check: *{2}(main+8) = 0x8000

# The branch counts instructions from the one after it, and the only
# instruction in between is the ret, so the displacement is 1.
# jitlink-check: (*{4}(main+16))[20:0] = 1

        .text
        .globl  main
        .type   main,@function
main:
        ldgp $29, 0($27)
        ldq  $27, callee($29)   !literal
        jsr  $26, ($27)
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
