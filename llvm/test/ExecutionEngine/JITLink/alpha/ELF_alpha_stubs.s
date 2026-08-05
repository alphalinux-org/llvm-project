# RUN: rm -rf %t && mkdir -p %t
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o %t/elf_stub.o %s
# RUN: llvm-jitlink -noexec -abs external_func=0x7fff00001234 \
# RUN:     -check=%s %t/elf_stub.o
#
# A branch to a symbol outside the graph goes through a stub.  Alpha cannot
# form a PC-relative address, so the stub takes the address of its own second
# instruction with a branch, reads the address of its GOT entry from the
# quadword that follows, and jumps to what that entry holds.

# jitlink-check: *{8}(stub_addr(elf_stub.o, external_func) + 16) = \
# jitlink-check:   got_addr(elf_stub.o, external_func)
# jitlink-check: *{8}got_addr(elf_stub.o, external_func) = external_func
# jitlink-check: (*{4}(stub_addr(elf_stub.o, external_func) + 0))[31:21] = 0x61b
# jitlink-check: *{4}(stub_addr(elf_stub.o, external_func) + 4) = 0xa77b000c
# jitlink-check: *{4}(stub_addr(elf_stub.o, external_func) + 8) = 0xa77b0000
# jitlink-check: *{4}(stub_addr(elf_stub.o, external_func) + 12) = 0x6bfb0000

# The branch reaches the stub.
# jitlink-check: (*{4}main)[20:0] = \
# jitlink-check:   ((stub_addr(elf_stub.o, external_func) - (main + 4)) >> 2)[20:0]

        .text
        .globl  main
        .type   main,@function
main:
        bsr  $26, external_func
        ret
        .size   main, .-main
