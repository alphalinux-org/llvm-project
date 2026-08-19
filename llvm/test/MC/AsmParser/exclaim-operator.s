## '!' is the GNU infix `or not' operator on every target that does not ask for
## it as a relocation-specifier separator, and stays one here: 3 | ~5 is -5.
# RUN: llvm-mc -triple=x86_64-unknown-linux-gnu -filetype=obj %s -o %t
# RUN: llvm-objdump -s -j .text %t | FileCheck %s

# CHECK: Contents of section .text:
# CHECK-NEXT: 0000 fbffffff
	.long 3!5
