; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 \
; RUN:   < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -O0 -global-isel \
; RUN:   -global-isel-abort=1 < %s | FileCheck %s --check-prefix=O0

; A switch dense enough to become a jump table.  Until this landed, G_BRJT and
; G_JUMP_TABLE were marked unsupported and every such function went to the
; SelectionDAG path whole -- the largest structural hole GlobalISel had left on
; this target, and the most ordinary construct in it.
;
; The sequence below is LowerBR_JT's, instruction for instruction, and it has
; to be: getJumpTableEncoding says EK_Custom32, so each entry is a 32-bit
; gp-relative offset written by AlphaAsmPrinter::emitJumpTableEntry, and a
; dispatch that read the entries any other way would read the wrong thing.
; That is also why the table address is formed gp-relative and why $29 is added
; back after the load -- and why `ldl`, which sign-extends the longword it
; reads, is the right load.
;
; -O0 has its own run line because the index reaches G_BRJT through a different
; path there and the legalizer rule has to accept both.

; CHECK-LABEL: sw:
; CHECK:       ldah $[[T:[0-9]+]], .LJTI0_0($29)		!gprelhigh
; CHECK-NEXT:  lda $[[T]], .LJTI0_0($[[T]])		!gprellow
; CHECK-NEXT:  s4addq ${{[0-9]+}}, $[[T]], $[[A:[0-9]+]]
; CHECK-NEXT:  ldl $[[O:[0-9]+]], 0($[[A]])
; CHECK-NEXT:  addq $29, $[[O]], $[[D:[0-9]+]]
; CHECK-NEXT:  jmp $31, ($[[D]]), 0

; The table itself: gp-relative 32-bit entries, one per case.
; CHECK:      .LJTI0_0:
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_
; CHECK-NEXT: .gprel32 .LBB0_

; O0-LABEL: sw:
; O0:       .LJTI0_0($29)		!gprelhigh
; O0:       jmp $31, ($
; O0:       .LJTI0_0:
; O0-NEXT:  .gprel32 .LBB0_

define i64 @sw(i64 %x) {
entry:
  switch i64 %x, label %d [ i64 0, label %a
                            i64 1, label %b
                            i64 2, label %c
                            i64 3, label %e
                            i64 4, label %f
                            i64 5, label %g
                            i64 6, label %h ]
a: ret i64 10
b: ret i64 20
c: ret i64 30
e: ret i64 40
f: ret i64 50
g: ret i64 60
h: ret i64 70
d: ret i64 0
}
