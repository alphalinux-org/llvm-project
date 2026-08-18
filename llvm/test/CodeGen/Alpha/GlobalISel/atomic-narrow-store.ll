; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 < %s \
; RUN:   | FileCheck %s --check-prefix=DAG

; An atomic narrow store takes the ldq_l/stq_c form on a pre-BWX subtarget
; whatever -msafe-bwa says.  The feature is about a *plain* store to a
; neighbouring field, which a program is only sometimes entitled to treat as a
; separate object; an atomic store is that entitlement unconditionally, so the
; plain read-modify-write would lose another thread's update to a different
; byte of the same quadword however the module was compiled.  The SelectionDAG
; path has always done this -- its atomic_store_8/16 patterns name
; SAFE_STOREI8/16 outright -- so check the two side by side.

; CHECK-LABEL: st8_seq:
; CHECK:       mb
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK:       mb
; CHECK-NOT:   stq_u
; DAG-LABEL: st8_seq:
; DAG:       ldq_l
; DAG:       stq_c
define void @st8_seq(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p seq_cst, align 1
  ret void
}

; CHECK-LABEL: st8_monotonic:
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK-NOT:   stq_u
; DAG-LABEL: st8_monotonic:
; DAG:       ldq_l
; DAG:       stq_c
define void @st8_monotonic(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p monotonic, align 1
  ret void
}

; CHECK-LABEL: st16_release:
; CHECK:       mb
; CHECK:       ldq_l
; CHECK:       stq_c
; CHECK-NOT:   stq_u
; DAG-LABEL: st16_release:
; DAG:       ldq_l
; DAG:       stq_c
define void @st16_release(ptr %p, i16 %v) {
  store atomic i16 %v, ptr %p release, align 2
  ret void
}

; A plain narrow store is unaffected: it keeps the cheap read-modify-write.
; CHECK-LABEL: st8_plain:
; CHECK:       stq_u
; CHECK-NOT:   stq_c
; DAG-LABEL: st8_plain:
; DAG:       stq_u
define void @st8_plain(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}
