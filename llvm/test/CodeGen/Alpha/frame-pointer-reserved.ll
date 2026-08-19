; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s \
; RUN:   --implicit-check-not='{{bis \$31, \$30, \$15}}'

@g = external global [16 x i64]
declare void @opaque()

; $15 is the frame pointer only where one is set up.  This function has no
; variable-sized object, so it has no frame pointer and $15 is available like
; any other callee-saved register; seven values live across the call need all
; of $9 through $15.
define void @no_frame_pointer_frees_r15() {
; CHECK-LABEL: no_frame_pointer_frees_r15:
; $15 is saved and used like any other callee-saved register.  That no frame
; pointer is set up anywhere in the function is what the implicit-check-not on
; the RUN line says; a CHECK-NOT here would stop at the save below it, which is
; before the point in the prologue a frame pointer would be established.
; CHECK:       stq $15,
  %p0 = getelementptr [16 x i64], ptr @g, i64 0, i64 0
  %p1 = getelementptr [16 x i64], ptr @g, i64 0, i64 1
  %p2 = getelementptr [16 x i64], ptr @g, i64 0, i64 2
  %p3 = getelementptr [16 x i64], ptr @g, i64 0, i64 3
  %p4 = getelementptr [16 x i64], ptr @g, i64 0, i64 4
  %p5 = getelementptr [16 x i64], ptr @g, i64 0, i64 5
  %p6 = getelementptr [16 x i64], ptr @g, i64 0, i64 6
  %p7 = getelementptr [16 x i64], ptr @g, i64 0, i64 7
  %v0 = load volatile i64, ptr %p0
  %v1 = load volatile i64, ptr %p1
  %v2 = load volatile i64, ptr %p2
  %v3 = load volatile i64, ptr %p3
  %v4 = load volatile i64, ptr %p4
  %v5 = load volatile i64, ptr %p5
  %v6 = load volatile i64, ptr %p6
  %v7 = load volatile i64, ptr %p7
  call void @opaque()
  store volatile i64 %v0, ptr %p0
  store volatile i64 %v1, ptr %p1
  store volatile i64 %v2, ptr %p2
  store volatile i64 %v3, ptr %p3
  store volatile i64 %v4, ptr %p4
  store volatile i64 %v5, ptr %p5
  store volatile i64 %v6, ptr %p6
  store volatile i64 %v7, ptr %p7
  ret void
}
