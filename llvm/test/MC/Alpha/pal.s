# RUN: llvm-mc -triple=alpha-unknown-linux-gnu --show-encoding %s | FileCheck %s

# A PALcode call with an explicit function code, as the kernel emits for its
# privileged PAL entries.
# CHECK: call_pal 129                     # encoding: [0x81,0x00,0x00,0x00]
	call_pal 0x81

# CHECK: call_pal 4194303                 # encoding: [0xff,0xff,0x3f,0x00]
	call_pal 0x3fffff

# The unprivileged PALcode functions have their own mnemonics (matching GNU as),
# each a call_pal with a fixed function code.
# CHECK: call_pal 0                       # encoding: [0x00,0x00,0x00,0x00]
	halt
# CHECK: call_pal 2                       # encoding: [0x02,0x00,0x00,0x00]
	draina
# CHECK: call_pal 128                     # encoding: [0x80,0x00,0x00,0x00]
	bpt
# CHECK: call_pal 129                     # encoding: [0x81,0x00,0x00,0x00]
	bugchk
# CHECK: call_pal 131                     # encoding: [0x83,0x00,0x00,0x00]
	callsys
# CHECK: call_pal 131                     # encoding: [0x83,0x00,0x00,0x00]
	chmk
# CHECK: call_pal 134                     # encoding: [0x86,0x00,0x00,0x00]
	imb
# CHECK: call_pal 158                     # encoding: [0x9e,0x00,0x00,0x00]
	rduniq
# CHECK: call_pal 159                     # encoding: [0x9f,0x00,0x00,0x00]
	wruniq
# CHECK: call_pal 170                     # encoding: [0xaa,0x00,0x00,0x00]
	gentrap

## The five PALmode-only primary opcodes.  A PALmode processor interprets the
## whole 26-bit remainder itself, so the mnemonic is the opcode and the operand
## is the raw function code -- which is how GNU as spells them, and these are
## the words gas produces for the same five lines.
# CHECK: pal19 4660                       # encoding: [0x34,0x12,0x00,0x64]
	pal19 0x1234
# CHECK: pal1b 1                          # encoding: [0x01,0x00,0x00,0x6c]
	pal1b 1
# CHECK: pal1d 2                          # encoding: [0x02,0x00,0x00,0x74]
	pal1d 2
# CHECK: pal1e 3                          # encoding: [0x03,0x00,0x00,0x78]
	pal1e 3
# CHECK: pal1f 4                          # encoding: [0x04,0x00,0x00,0x7c]
	pal1f 4

## The function code is a 26-bit field and there is nowhere to put anything
## wider, so a value that does not fit is refused rather than truncated to
## call_pal 0.
# RUN: not llvm-mc -triple=alpha-unknown-linux-gnu %s --defsym RANGE=1 -o /dev/null 2>&1 \
# RUN:   | FileCheck %s --check-prefix=RANGE
.ifdef RANGE
	call_pal 0x4000000
	call_pal -1
	pal19 0x4000000
.endif
# RANGE: error: PALcode function code must be a 26-bit unsigned value
# RANGE: error: PALcode function code must be a 26-bit unsigned value
# RANGE: error: PALcode function code must be a 26-bit unsigned value
