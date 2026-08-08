; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+reserve-r8 < %s \
; RUN:   | FileCheck %s
; RUN: not llc -mtriple=alpha-unknown-linux-gnu < %s 2>&1 \
; RUN:   | FileCheck %s --check-prefix=ERR

; A global register variable bound to a numbered register (the kernel uses
; "register T *p __asm__("$8")" for the current-thread pointer) reads that
; register directly.  The register has to be reserved -- with -ffixed-$8, which
; reaches the back end as +reserve-r8 -- or the allocator is free to put
; something else there and the variable reads whatever that was.
; ERR: Trying to obtain non-reserved register "$8"

; CHECK-LABEL: cur:
; CHECK: bis $31, $8, $0
define i64 @cur() {
  %v = call i64 @llvm.read_register.i64(metadata !0)
  ret i64 %v
}

declare i64 @llvm.read_register.i64(metadata)

!llvm.named.register.$8 = !{!0}
!0 = !{!"$8"}
