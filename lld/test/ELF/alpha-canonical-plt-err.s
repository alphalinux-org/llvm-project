# REQUIRES: alpha
## Taking the address of a function defined in a shared object from a non-PIE
## needs a canonical PLT entry to stand in for it wherever the address cannot
## be written at run time.  Alpha has no PLT -- a call loads the callee out of
## the ordinary GOT -- so there is no entry to hand out, and the link is
## diagnosed rather than producing an empty one.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu \
# RUN:   %S/Inputs/alpha-shared-fn.s -o %t1.o
# RUN: ld.lld -shared -soname=t1.so %t1.o -o %t1.so

# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu --defsym RO=1 %s -o %t-ro.o
# RUN: not ld.lld %t-ro.o %t1.so -o /dev/null 2>&1 | FileCheck %s
# CHECK: error: cannot create a canonical PLT entry for fn; recompile with -fPIC

## In a writable section there is no need for one: the address is filled in by
## a dynamic relocation, which is also why the common case does not reach the
## diagnostic above.
# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t.o
# RUN: ld.lld %t.o %t1.so -o %t.exe
# RUN: llvm-readobj -r %t.exe | FileCheck %s --check-prefix=DATA
# DATA:      .rela.dyn {
# DATA-NEXT:   R_ALPHA_REFQUAD fn 0x0
# DATA-NEXT: }

	.text
	.globl _start
_start:
	ret

.ifdef RO
	.section .rodata,"a",@progbits
.else
	.data
.endif
	.globl p
p:
	.quad fn
