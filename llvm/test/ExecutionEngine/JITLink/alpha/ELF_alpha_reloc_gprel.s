# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_reloc.o %s
# RUN: llvm-jitlink -noexec -check=%s %t/elf_reloc.o
#
# Check R_ALPHA_GPRELHIGH and R_ALPHA_GPRELLOW, the two halves of a
# displacement from the global pointer, and R_ALPHA_LITERAL, which reads the
# GOT entry for a symbol.  The global pointer addresses a 64k window centered on
# the GOT, so it sits 0x8000 past its start; the low half of a displacement is
# signed, so its sign bit is carried into the high half.

# jitlink-check: (*{2}(main+0))[15:0] = \
# jitlink-check:   (((datum - (section_addr(elf_reloc.o, $__GOT) + 0x8000)) + 0x8000) >> 16) & 0xffff
# jitlink-check: (*{2}(main+4))[15:0] = \
# jitlink-check:   (datum - (section_addr(elf_reloc.o, $__GOT) + 0x8000)) & 0xffff

# The one GOT entry sits at the start of the section, 0x8000 below the global
# pointer.
# jitlink-check: (*{2}(main+8))[15:0] = 0x8000

        .text
        .globl  main
        .type   main,@function
main:
        ldah $1, datum($29)     !gprelhigh
        lda  $1, datum($1)      !gprellow
        ldq  $27, datum($29)    !literal
        ret
        .size   main, .-main

        .data
        .globl  datum
        .p2align 3
datum:
        .quad   42
        .size   datum, 8
