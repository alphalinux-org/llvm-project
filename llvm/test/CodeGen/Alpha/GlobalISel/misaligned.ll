; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 \
; RUN:   -mcpu=ev6 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 \
; RUN:   -mcpu=ev6 -mattr=+safe-partial < %s | FileCheck %s --check-prefix=SAFE

; A misaligned access can straddle a quadword boundary, so it takes both of the
; quadwords it can fall in.  GlobalISel builds the same expansion the
; SelectionDAG path does; -global-isel-abort=1 is the point of these run lines,
; because a fallback would give the SelectionDAG output and pass every check
; below without GlobalISel having selected anything.
;
; A load reads both quadwords, extracts the part of the field each holds and
; splices the halves together.  When the field does not straddle, the second
; ldq_u reads the same quadword again and the high extract contributes zero.

; CHECK-LABEL: load_misaligned_i64:
; CHECK:      ldq_u
; CHECK:      ldq_u
; CHECK:      extql
; CHECK:      extqh
; CHECK:      bis
define i64 @load_misaligned_i64(ptr %p) {
  %v = load i64, ptr %p, align 1
  ret i64 %v
}

; The extract instructions come in a width for two, four and eight bytes.

; CHECK-LABEL: load_misaligned_i32:
; CHECK:      extll
; CHECK:      extlh
define i32 @load_misaligned_i32(ptr %p) {
  %v = load i32, ptr %p, align 1
  ret i32 %v
}

; CHECK-LABEL: load_misaligned_i16:
; CHECK:      extwl
; CHECK:      extwh
define i16 @load_misaligned_i16(ptr %p) {
  %v = load i16, ptr %p, align 1
  ret i16 %v
}

; There is no extract from a floating register, so a misaligned floating access
; moves through an integer one -- in the same S_floating form the aligned lds
; converts on its way, which is what itofs does.

; CHECK-LABEL: load_misaligned_float:
; CHECK:      extll
; CHECK:      extlh
; CHECK:      bis
; CHECK:      itofs
define float @load_misaligned_float(ptr %p) {
  %v = load float, ptr %p, align 1
  ret float %v
}

; CHECK-LABEL: load_misaligned_double:
; CHECK:      extql
; CHECK:      extqh
; CHECK:      itoft
define double @load_misaligned_double(ptr %p) {
  %v = load double, ptr %p, align 1
  ret double %v
}

; A store reads both quadwords, masks the field out of each, splices the value
; in and writes both back.  It must update both: an access that wrote only the
; quadword the address falls in would lose the part of the field beyond the
; boundary.

; CHECK-LABEL: store_misaligned_i16:
; CHECK:      ldq_u
; CHECK:      ldq_u
; CHECK:      mskwh
; CHECK:      inswh
; CHECK:      stq_u
; CHECK:      mskwl
; CHECK:      inswl
; CHECK:      stq_u
define void @store_misaligned_i16(ptr %p, i16 %v) {
  store i16 %v, ptr %p, align 1
  ret void
}

; CHECK-LABEL: store_misaligned_float:
; CHECK:      ftois
; CHECK:      stq_u
; CHECK:      stq_u
define void @store_misaligned_float(ptr %p, float %v) {
  store float %v, ptr %p, align 1
  ret void
}

; With -msafe-partial each spanned quadword is updated by a lock-based retry
; loop instead, so that the read-modify-write is atomic against another thread
; writing a different field of the same quadword.

; SAFE-LABEL: store_misaligned_i64:
; SAFE:      ldq_l
; SAFE:      stq_c
; CHECK-LABEL: store_misaligned_i64:
; CHECK-NOT:  ldq_l
; CHECK:      stq_u
define void @store_misaligned_i64(ptr %p, i64 %v) {
  store i64 %v, ptr %p, align 1
  ret void
}
