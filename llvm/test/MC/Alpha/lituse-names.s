# RUN: llvm-mc -filetype=obj -triple=alpha-unknown-linux-gnu %s -o %t
# RUN: llvm-objdump -dr %t | FileCheck %s

## Every lituse GNU as accepts, with the addend it assigns.  The addend is the
## whole content of an R_ALPHA_LITUSE: it says what use the instruction makes
## of the literal loaded before it, which is how bfd decides whether the pair
## can be relaxed.  The numbers are LITUSE_ALPHA_* from include/elf/alpha.h,
## where addr is 0 -- so a use type of zero is a real one, not "none".
## lituse_jsrdirect is the one gcc emits itself, on the millicode call behind
## every integer / and %, so rejecting it failed 16 of 237 gcc -O2 translation
## units with "unknown relocation name".

# CHECK:      ldq $1, 0($29)
# CHECK-NEXT: R_ALPHA_LITERAL sym
# CHECK-NEXT: addq $1, $31, $1
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*{{$}}
	ldq $1, sym($29)	!literal!1
	addq $1, $31, $1	!lituse_addr!1

# CHECK-NEXT: ldq $1, 0($1)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x1
	ldq $1, 0($1)		!lituse_base!1

# CHECK-NEXT: addq $1, $2, $3
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x2
	addq $1, $2, $3		!lituse_bytoff!1

# CHECK-NEXT: jsr $26, ($27)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x3
	jsr $26, ($27)		!lituse_jsr!1

# CHECK-NEXT: lda $16, 0($29)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x4
	lda $16, 0($29)		!lituse_tlsgd!1

# CHECK-NEXT: lda $16, 0($29)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x5
	lda $16, 0($29)		!lituse_tlsldm!1

## gcc's %j: the call to __divqu and friends, whose target the linker may turn
## into a direct branch without a hint.
# CHECK-NEXT: jsr $23, ($23)
# CHECK-NEXT: R_ALPHA_LITUSE *ABS*+0x6
	jsr $23, ($23)		!lituse_jsrdirect!1
