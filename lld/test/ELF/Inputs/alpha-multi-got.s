## 8192 GOT entries, which is exactly as many as one gp reaches, so whatever is
## linked after this file has to start a partition of its own.
.text
.globl f1
f1:
	ldgp $29, 0($27)
.irpc a, 01
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.irpc e, 01234567
	ldq $1, s\a\b\c\d\e($29)	!literal
.endr
.endr
.endr
.endr
.endr
	ret
.data
.irpc a, 01
.irpc b, 01234567
.irpc c, 01234567
.irpc d, 01234567
.irpc e, 01234567
.globl s\a\b\c\d\e
s\a\b\c\d\e:	.quad 0
.endr
.endr
.endr
.endr
.endr
