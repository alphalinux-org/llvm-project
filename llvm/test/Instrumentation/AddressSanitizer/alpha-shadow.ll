; Check the Alpha shadow mapping: scale 3, constant offset 0x30000000000,
; matching ASAN_SHADOW_OFFSET_CONST for SANITIZER_ALPHA in
; compiler-rt/lib/asan/asan_mapping.h. The offset is not a power of two, so the
; shadow address must be computed with an add rather than an or.
;
; The offset has to leave the whole shadow inside the address space the kernel
; gives a process: alpha's TASK_SIZE is 0x40000000000
; (arch/alpha/include/asm/processor.h), and with scale 3 the shadow spans
; 512GB from the offset, so [0x30000000000, 0x38000000000) fits and anything at
; or above 0x38000000000 does not.  qemu-alpha in user mode does not enforce
; the guest TASK_SIZE, so only a real kernel rejects a bad choice.
;
; RUN: opt < %s -passes=asan -S -mtriple=alpha-unknown-linux-gnu | FileCheck %s

define i32 @read(ptr %a) sanitize_address {
entry:
  %tmp1 = load i32, ptr %a, align 4
  ret i32 %tmp1
}
; CHECK-LABEL: @read
; CHECK: lshr {{.*}} 3
; CHECK-NEXT: add {{.*}} 3298534883328
; CHECK-NOT: or i64
; CHECK: ret
