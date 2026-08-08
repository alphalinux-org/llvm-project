# RUN: llvm-mc -triple=alpha-unknown-linux-gnu %s | FileCheck %s --check-prefix=NONE
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+ieee %s | FileCheck %s --check-prefix=SU
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+ieee-with-inexact %s | FileCheck %s --check-prefix=SUI
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+fptrap-u %s | FileCheck %s --check-prefix=U
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+fpround-dynamic %s | FileCheck %s --check-prefix=DYN
# RUN: llvm-mc -triple=alpha-unknown-linux-gnu -mattr=+ieee,+fpround-dynamic %s | FileCheck %s --check-prefix=SUD

	addt $f1, $f2, $f3
	cmpteq $f1, $f2, $f3
	cvtqt $f1, $f3

# NONE: addt $f1
# NONE: cmpteq $f1
# NONE: cvtqt $f1

# The arithmetic op takes /su, the compare /su, integer-to-float only inexact.
# SU: addt/su $f1
# SU: cmpteq/su $f1
# SU: cvtqt $f1

# SUI: addt/sui $f1
# SUI: cmpteq/su $f1
# SUI: cvtqt/sui $f1

# The bare underflow mode applies only to arithmetic.
# U: addt/u $f1
# U: cmpteq $f1
# U: cvtqt $f1

# Rounding applies to arithmetic and integer-to-float, not to compares.
# DYN: addt/d $f1
# DYN: cmpteq $f1
# DYN: cvtqt/d $f1

# Trap and rounding qualifiers combine.
# SUD: addt/sud $f1
# SUD: cmpteq/su $f1
# SUD: cvtqt/d $f1
