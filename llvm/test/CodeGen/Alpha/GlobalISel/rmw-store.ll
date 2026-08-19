; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev4 -global-isel \
; RUN:   -global-isel-abort=1 -stop-after=instruction-select < %s | FileCheck %s

; Without BWX a narrow store is a read-modify-write of the whole quadword the
; field lives in.  Both RMW pseudos read and write all eight bytes, so the
; memory operand is widened to match, exactly as the SelectionDAG path widens
; it: carrying the original one- or two-byte reference would understate the
; footprint to anything that later asks whether the store can alias another.

; CHECK-LABEL: name: st8
; CHECK: RMW_STOREI8 {{.*}} :: (load store (s64)
define void @st8(ptr %p, i8 %v) {
  store i8 %v, ptr %p
  ret void
}

; CHECK-LABEL: name: st16
; CHECK: RMW_STOREI16 {{.*}} :: (load store (s64)
define void @st16(ptr %p, i16 %v) {
  store i16 %v, ptr %p, align 2
  ret void
}
