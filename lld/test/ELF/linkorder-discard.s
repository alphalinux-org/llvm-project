# REQUIRES: x86
## A SHF_LINK_ORDER section whose linked-to section is discarded is discarded
## with it. Without that, the section is emitted with an sh_link that no longer
## designates an output section and the link fails.

# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=x86_64 a.s -o a.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 b.s -o b.o
# RUN: llvm-mc -filetype=obj -triple=x86_64 main.s -o main.o

## b.o's group is discarded, and so is the __patchable_function_entries section
## that points into it. Only a.o's entry is left.
# RUN: ld.lld main.o a.o b.o -o out
# RUN: llvm-readelf -S out | FileCheck %s \
# RUN:     --implicit-check-not='{{ }}__patchable_function_entries '
# CHECK: {{ }}__patchable_function_entries PROGBITS {{.*}} 000008

## Same in a relocatable link and with --emit-relocs, where the relocation
## sections are copied to the output and must not be left with a dangling
## sh_info. Exactly one of each survives, and the surviving relocation section
## points at the surviving __patchable_function_entries.
# RUN: ld.lld -r main.o a.o b.o -o out.ro
# RUN: llvm-readelf -S out.ro | FileCheck %s --check-prefix=REL \
# RUN:     --implicit-check-not='{{ }}__patchable_function_entries ' \
# RUN:     --implicit-check-not='{{ }}.rela__patchable_function_entries '
# REL: [[#%u,PFE:]]] __patchable_function_entries PROGBITS
# REL: {{ }}.rela__patchable_function_entries RELA {{.*}} I {{[0-9]+}} [[#%u,PFE]]

# RUN: ld.lld --emit-relocs main.o a.o b.o -o out.er
# RUN: llvm-readelf -S out.er | FileCheck %s --check-prefix=REL \
# RUN:     --implicit-check-not='{{ }}__patchable_function_entries ' \
# RUN:     --implicit-check-not='{{ }}.rela__patchable_function_entries '

## A chain of SHF_LINK_ORDER sections is discarded as a whole, even when a
## section precedes the one it links to.
# RUN: yaml2obj chain.yaml -o chain.o
# RUN: ld.lld -r main.o a.o chain.o -o out.chain
# RUN: llvm-readelf -S out.chain | FileCheck %s --check-prefix=CHAIN \
# RUN:     --implicit-check-not='{{ }}lo_inner ' \
# RUN:     --implicit-check-not='{{ }}lo_outer '
# CHAIN: {{ }}.text PROGBITS

## A relocation section that precedes the SHF_LINK_ORDER section it relocates
## must be dropped along with it. yaml2obj is used because assemblers place
## relocation sections last.
# RUN: yaml2obj rev.yaml -o rev.o
# RUN: ld.lld -r main.o a.o rev.o -o out.rev
# RUN: llvm-readelf -S out.rev | FileCheck %s --check-prefix=REL \
# RUN:     --implicit-check-not='{{ }}__patchable_function_entries ' \
# RUN:     --implicit-check-not='{{ }}.rela__patchable_function_entries '

## A section linked to an SHF_EXCLUDE'd section is discarded with it in a final
## link, and kept, as SHF_EXCLUDE itself is, in a relocatable link. GNU ld does
## the same.
# RUN: llvm-mc -filetype=obj -triple=x86_64 excl.s -o excl.o
# RUN: ld.lld main.o a.o excl.o -o out.excl
# RUN: llvm-readelf -S out.excl | FileCheck %s \
# RUN:     --implicit-check-not='{{ }}__patchable_function_entries ' \
# RUN:     --implicit-check-not='{{ }}.text.X '
# RUN: ld.lld -r main.o a.o excl.o -o out.excl.ro
# RUN: llvm-readelf -S out.excl.ro | FileCheck %s --check-prefix=EXCLREL
# EXCLREL: [[#%u,X:]]] .text.X PROGBITS {{.*}} AXE
# EXCLREL: {{ }}__patchable_function_entries PROGBITS {{.*}} WAL [[#%u,X]]

#--- a.s
.globl fa
fa:
  retq

.section .text.P,"axG",@progbits,P,comdat
.globl P
P:
  retq

.section __patchable_function_entries,"awo",@progbits,.text.P
  .quad P

#--- b.s
.globl fb
fb:
  retq

.section .text.P,"axG",@progbits,P,comdat
.globl P
P:
  retq

.section __patchable_function_entries,"awo",@progbits,.text.P
  .quad P

#--- main.s
.globl _start
_start:
  callq P
  callq fa
  retq

#--- excl.s
.section .text.X,"axe",@progbits
X:
  retq

.section __patchable_function_entries,"awo",@progbits,.text.X
  .quad X

#--- chain.yaml
## lo_outer links to lo_inner, which links to the discarded .text.P. lo_outer
## comes first, so a single pass over the section table would keep it.
--- !ELF
FileHeader:
  Class:   ELFCLASS64
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: EM_X86_64
Sections:
  - Name:    .text.P
    Type:    SHT_PROGBITS
    Flags:   [ SHF_ALLOC, SHF_EXECINSTR, SHF_GROUP ]
    Size:    1
  - Name:    .group
    Type:    SHT_GROUP
    Link:    .symtab
    Info:    P
    Members:
      - SectionOrType: GRP_COMDAT
      - SectionOrType: .text.P
  - Name:    lo_outer
    Type:    SHT_PROGBITS
    Flags:   [ SHF_ALLOC, SHF_LINK_ORDER ]
    Link:    lo_inner
    Size:    8
  - Name:    lo_inner
    Type:    SHT_PROGBITS
    Flags:   [ SHF_ALLOC, SHF_LINK_ORDER ]
    Link:    .text.P
    Size:    8
Symbols:
  - Name:    P
    Section: .text.P
    Binding: STB_GLOBAL

#--- rev.yaml
--- !ELF
FileHeader:
  Class:   ELFCLASS64
  Data:    ELFDATA2LSB
  Type:    ET_REL
  Machine: EM_X86_64
Sections:
  - Name:    .text.P
    Type:    SHT_PROGBITS
    Flags:   [ SHF_ALLOC, SHF_EXECINSTR, SHF_GROUP ]
    Size:    1
  - Name:    .group
    Type:    SHT_GROUP
    Link:    .symtab
    Info:    P
    Members:
      - SectionOrType: GRP_COMDAT
      - SectionOrType: .text.P
  - Name:    .rela__patchable_function_entries
    Type:    SHT_RELA
    Link:    .symtab
    Info:    __patchable_function_entries
    Relocations:
      - Offset: 0
        Symbol: P
        Type:   R_X86_64_64
  - Name:    __patchable_function_entries
    Type:    SHT_PROGBITS
    Flags:   [ SHF_ALLOC, SHF_WRITE, SHF_LINK_ORDER ]
    Link:    .text.P
    Size:    8
Symbols:
  - Name:    P
    Section: .text.P
    Binding: STB_GLOBAL
