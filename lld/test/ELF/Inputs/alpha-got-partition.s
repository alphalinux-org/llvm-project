## 3584 GOT entries: less than half a partition, so a partition holding only
## this file is still mostly empty.
.text
.globl f1
.usepv f1, std
f1:
	ldgp $29, 0($27)
.irpc a, 0123456
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
.irpc a, 0123456
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.globl p\a\b\c\d
p\a\b\c\d:	.quad 0
.endr
.endr
.endr
.endr
