## The 's' section flag is target-specific: SHF_HEX_GPREL on Hexagon and
## SHF_ALPHA_GPREL on Alpha.  Everywhere else it is not a flag at all, and
## giving it has to stay an error rather than quietly setting one of those.
# RUN: not llvm-mc -triple=x86_64-unknown-linux-gnu -filetype=obj %s -o /dev/null \
# RUN:   2>&1 | FileCheck %s

# CHECK: error: unknown flag
	.section .foo,"as",@progbits
