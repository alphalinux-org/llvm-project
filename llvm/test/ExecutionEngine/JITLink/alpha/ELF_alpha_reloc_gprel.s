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
# pointer.  That displacement is a GPRel16 fixup: a literal whose GOT entry is
# in range becomes one, which is the only way the 16-bit form is reached -- a
# datum reference far enough from the global pointer is rejected rather than
# truncated.
# jitlink-check: (*{2}(main+8))[15:0] = 0x8000

# R_ALPHA_GPREL32 is the same displacement as a four-byte datum rather than an
# instruction field, which is how a jump table in .data names its targets.
# jitlink-check: *{4}(tbl) = \
# jitlink-check:   (datum - (section_addr(elf_reloc.o, $__GOT) + 0x8000)) & 0xffffffff

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
        .p2align 2
        .globl  tbl
tbl:
        .gprel32 datum

        .globl  datum
        .p2align 3
datum:
        .quad   42
        .size   datum, 8
