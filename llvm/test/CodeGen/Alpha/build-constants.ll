; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s \
; RUN:   | FileCheck %s --check-prefix=POOL
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+build-constants -O2 < %s \
; RUN:   | FileCheck %s --check-prefix=INLINE

; By default a wide constant is loaded GP-relative from the constant pool.
; With +build-constants (gcc's -mbuild-constants) it is materialized entirely in
; code -- ldah/lda of each 32-bit half joined by a shift -- so nothing in the
; data segment is referenced.

; POOL-LABEL: wide:
; POOL:      ldah {{.*}}!gprelhigh
; POOL:      ldq {{.*}}!gprellow

; INLINE-LABEL: wide:
; INLINE:      ldah
; INLINE:      lda
; INLINE:      sll {{\$[0-9]+}}, 32, {{\$[0-9]+}}
; INLINE:      ldah
; INLINE:      lda
; INLINE-NOT:  !gprel
define i64 @wide() {
  ret i64 81985529216486896 ; 0x0123456789abcdef0 truncated to 0x123456789abcdef0
}

; The largest magnitudes exercise the ldah carry (a 0x8000 high half).
; INT64_MAX is (-0x8000 << 16) << 32, less one: the high half is 0x8000, which
; does not fit ldah's signed field, so it is written negative and the low half
; carries.  Checking only that no pool load appears would pass for any constant
; at all, and these two are the cases the carry is about.
; INLINE-LABEL: maxpos:
; INLINE:      ldah $0, -32768($31)
; INLINE-NEXT: sll $0, 32, $0
; INLINE-NEXT: lda $0, -1($0)
; INLINE-NOT:  !gprel
define i64 @maxpos() {
  ret i64 9223372036854775807 ; INT64_MAX
}

; INT64_MIN is the same without the carry.
; INLINE-LABEL: minneg:
; INLINE:      ldah $0, -32768($31)
; INLINE-NEXT: sll $0, 32, $0
; INLINE-NOT:  lda $0
; INLINE-NOT:  !gprel
define i64 @minneg() {
  ret i64 -9223372036854775808 ; INT64_MIN
}
