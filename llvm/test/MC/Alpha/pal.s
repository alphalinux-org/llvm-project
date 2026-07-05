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
