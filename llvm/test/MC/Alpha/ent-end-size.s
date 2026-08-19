# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-readelf -s %t | FileCheck %s

## GNU as gives a procedure its st_size from `.ent' to `.end'.  Every function
## gcc compiles is written that way and nothing else states the size, so
## without this each one is a FUNC of size 0 and neither gdb nor a profiler can
## say which function an address falls in.

# CHECK: {{[0-9]+}}: 0000000000000000     8 FUNC    GLOBAL DEFAULT {{.*}} one
# CHECK: {{[0-9]+}}: 0000000000000008    12 FUNC    GLOBAL DEFAULT {{.*}} two

	.text
	.globl one
	.ent one
one:
	nop
	ret $31, ($26), 1
	.end one

	.globl two
	.ent two
two:
	nop
	nop
	ret $31, ($26), 1
	.end two

## A size stated outright still wins: .end is where the size comes from only
## when nothing else provided one.
# CHECK: {{[0-9]+}}: 0000000000000014     4 FUNC    GLOBAL DEFAULT {{.*}} three
	.globl three
	.ent three
three:
	ret $31, ($26), 1
	.end three
	.size three, 4
