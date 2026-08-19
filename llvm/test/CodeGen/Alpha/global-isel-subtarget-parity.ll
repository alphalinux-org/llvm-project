; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-bwa < %s \
; RUN:   | FileCheck %s --check-prefix=SAFEBWA
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+safe-bwa -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=SAFEBWA

; A subtarget feature the GlobalISel path used to ignore.  Both RUN lines check
; the same output, so the two paths have to agree.

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
