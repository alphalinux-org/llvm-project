; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s

; A block address (taken with &&label for a computed goto) is stored and later
; jumped through indirectly.  The address is emitted as a relocated label.

@blkaddr = global ptr blockaddress(@cg, %target)

; CHECK-LABEL: cg:
; CHECK: jmp $31, ($16), 0
define void @cg(ptr %p) {
  indirectbr ptr %p, [label %target]
target:
  ret void
}

; Taking the address in code is what LowerBlockAddress does; the global above
; only needs a relocation in the data section, so it reaches none of it.
; CHECK-LABEL: take:
; CHECK:       ldah $0, .Ltmp{{[0-9]+}}($29){{.*}}!gprelhigh
; CHECK:       lda $0, .Ltmp{{[0-9]+}}($0){{.*}}!gprellow
define ptr @take() {
  ret ptr blockaddress(@cg, %target)
}

; CHECK-LABEL: blkaddr:
; CHECK: .quad .Ltmp0
