; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; __builtin_return_address and __builtin_frame_address, which reach the
; selector as intrinsics.  The register bank info gave a G_INTRINSIC no mapping
; at all, so a function taking either one fell back to the SelectionDAG path.
; What is produced is what that path produces (see LowerRETURNADDR).

define ptr @ra0() {
; CHECK-LABEL: ra0:
; CHECK: bis $31, $26, $0
  %r = call ptr @llvm.returnaddress(i32 0)
  ret ptr %r
}

; A frame carries no link to the one that called it, so a deeper frame would
; need an unwinder; the answer is zero, and no frame of our own is set up.
define ptr @ra1() {
; CHECK-LABEL: ra1:
; CHECK: bis $31, 0, $0
; CHECK-NOT: $15
  %r = call ptr @llvm.returnaddress(i32 1)
  ret ptr %r
}

; Taking the frame address forces a frame pointer, so $15 is set up and read.
define ptr @fa0() {
; CHECK-LABEL: fa0:
; CHECK: bis $31, $30, $15
; CHECK: bis $31, $15, $0
  %r = call ptr @llvm.frameaddress.p0(i32 0)
  ret ptr %r
}

define ptr @fa2() {
; CHECK-LABEL: fa2:
; CHECK: bis $31, 0, $0
  %r = call ptr @llvm.frameaddress.p0(i32 2)
  ret ptr %r
}

declare ptr @llvm.returnaddress(i32)
declare ptr @llvm.frameaddress.p0(i32)
