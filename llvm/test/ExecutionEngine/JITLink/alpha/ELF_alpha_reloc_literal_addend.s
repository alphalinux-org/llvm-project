# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_reloc.o %s
# RUN: llvm-jitlink -noexec -check=%s %t/elf_reloc.o
#
# An R_ALPHA_LITERAL against sym+N needs a GOT entry holding sym+N, and the
# instruction's displacement is then just that entry's distance from gp.  The
# addend must not survive on the edge: added to the displacement it would index
# off the GOT slot as though the slot were the object.
#
# This is not a corner case.  An assembler turns a reference to a local symbol
# into a section symbol plus an addend, so a literal naming sym+N is what an
# ordinary object file contains.  lld keys its GOT entries on the addend for
# the same reason.

        .text
        .globl  main
        .type   main,@function
main:
        ldq  $1, datum($29)     !literal
        ldq  $2, datum+8($29)   !literal
        ldq  $3, datum+16($29)  !literal
# A repeat of an earlier addend shares that entry rather than allocating one.
        ldq  $4, datum+8($29)   !literal
        ret
        .size   main, .-main

# Follow each displacement to the entry it names and check what that entry
# holds.  gp is 0x8000 past the start of the GOT, so the displacement plus
# 0x8000 is the entry's offset within the section.  Checking through the
# displacement rather than at a fixed offset keeps this independent of the
# order the entries happen to be laid out in.
# jitlink-check: *{8}(section_addr(elf_reloc.o, $__GOT) + \
# jitlink-check:   ((*{2}(main+0))[15:0] - 0x8000)) = datum
# jitlink-check: *{8}(section_addr(elf_reloc.o, $__GOT) + \
# jitlink-check:   ((*{2}(main+4))[15:0] - 0x8000)) = datum + 8
# jitlink-check: *{8}(section_addr(elf_reloc.o, $__GOT) + \
# jitlink-check:   ((*{2}(main+8))[15:0] - 0x8000)) = datum + 16

# The repeat shares the entry the second instruction used.
# jitlink-check: (*{2}(main+12))[15:0] = (*{2}(main+4))[15:0]

        .data
        .globl  datum
        .p2align 3
datum:
        .quad   42
        .quad   43
        .quad   44
        .size   datum, 24
