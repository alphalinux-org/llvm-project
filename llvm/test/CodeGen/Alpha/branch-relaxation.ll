; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; A branch carries a 21-bit displacement (+/- 4 MiB).  When the target is beyond
; that (here forced with a large inline-asm block), the branch is relaxed: the
; condition is inverted to skip an indirect jump that forms the far block's
; address gp-relatively and jumps through the scratch register $28.  The global
; pointer is established in the prologue so the gp-relative address is valid.

; CHECK-LABEL: far:
; CHECK:      ldgp $29, 0($27)
; CHECK:      bne $0, .LBB0_1
; CHECK:      ldah $28, .LBB0_2($29){{.*}}!gprelhigh
; CHECK-NEXT: lda $28, .LBB0_2($28){{.*}}!gprellow
; CHECK-NEXT: jmp $31, ($28), 0
define i64 @far(i64 %x) {
entry:
  %c = icmp eq i64 %x, 0
  br i1 %c, label %done, label %work
work:
  call void asm sideeffect ".space 5000000", ""()
  br label %done
done:
  ret i64 %x
}
