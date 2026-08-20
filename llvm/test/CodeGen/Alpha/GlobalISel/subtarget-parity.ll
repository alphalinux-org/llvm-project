; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-bwa < %s \
; RUN:   | FileCheck %s --check-prefix=SAFEBWA
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-bwa -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=SAFEBWA
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+build-constants < %s \
; RUN:   | FileCheck %s --check-prefix=BUILDC
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+build-constants -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=BUILDC

; Two subtarget features both instruction selectors have to honour.  Both RUN
; lines of each pair check the same output, so the two paths have to agree.

; Without BWX a byte store is a read-modify-write of the quadword holding it.
; The plain form is not atomic against another thread writing a different field
; of the same quadword; -msafe-bwa asks for the lock-based one instead, and
; RMW_STOREI8 carries a predicate saying it must not be selected here.
; SAFEBWA-LABEL: store_i8:
; SAFEBWA:       ldq_l
; SAFEBWA:       stq_c
; SAFEBWA-NOT:   ldq_u
define void @store_i8(ptr %p, i8 %v) {
  store i8 %v, ptr %p, align 1
  ret void
}

; -mbuild-constants materializes a wide constant inline rather than fetching it
; from the constant pool, which would need a global pointer: the dynamic loader
; runs before its own is established.
; BUILDC-LABEL: wide:
; BUILDC:       ldah $0, 4660($31)
; BUILDC-NEXT:  lda $0, 22137($0)
; BUILDC-NEXT:  sll $0, 32, $0
; BUILDC-NEXT:  ldah $0, -25923($0)
; BUILDC-NEXT:  lda $0, -8464($0)
; BUILDC-NOT:   gprel
define i64 @wide() {
  ret i64 1311768467463790320
}
