## 5120 GOT entries: enough that a file needing 4096 more cannot share the
## partition with it.
.text
.globl f1
.usepv f1, std
f1:
	ldgp $29, 0($27)
.irpc a, 0123456789
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
	ldq $1, p\a\b\c\d($29)	!literal
.endr
.endr
.endr
.endr
	ret
.data
.irpc a, 0123456789
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.globl p\a\b\c\d
p\a\b\c\d:	.quad 0
.endr
.endr
.endr
.endr
