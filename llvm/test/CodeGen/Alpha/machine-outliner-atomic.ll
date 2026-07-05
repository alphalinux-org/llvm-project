; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; The ldq_l/stq_c loops are built in addPreEmitPass2, after the outliner has
; run, so the outliner never sees a reservation window it could cut in half.
; A taken branch or a subroutine call between the load locked and the store
; conditional is allowed to clear the lock flag, so a loop with one in it would
; never make progress on real hardware -- and no emulator models that, which is
; why this has to be checked in the assembly.

; The whole atomic operation is outlined as the single instruction it still is
; at outlining time, so the window stays intact inside the outlined function.

; CHECK-LABEL: f1:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; CHECK-NEXT: ret
; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK: ldq_l
; CHECK-NOT: bsr
; CHECK-NOT: jmp
; CHECK: stq_c
; CHECK-NEXT: beq

define i8 @f1(ptr %p, i8 %v) minsize {
  %r = atomicrmw add ptr %p, i8 %v seq_cst
  ret i8 %r
}

define i8 @f2(ptr %p, i8 %v) minsize {
  %r = atomicrmw add ptr %p, i8 %v seq_cst
  ret i8 %r
}

define i8 @f3(ptr %p, i8 %v) minsize {
  %r = atomicrmw add ptr %p, i8 %v seq_cst
  ret i8 %r
}
