; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-text < %s \
; RUN:   | FileCheck %s --check-prefix=SMALLTEXT
; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s \
; RUN:   | llvm-objdump -dr - | FileCheck %s --check-prefix=OBJ

; The unwinder enters a landing pad directly, so the ldgp that follows the
; invoke's jsr never runs on the unwind path and $29 still holds the unwinder's
; global pointer.  A landing pad has to rebuild $29 before it can reach anything
; through the GOT, and it cannot do so from an incoming register: libgcc leaves
; the handler's address in $26, but libunwind restores $26 from the frame's own
; unwind info.  `br` supplies the base the !gpdisp pair needs by itself.

declare void @may_throw()
declare void @use(ptr)
declare i32 @__gxx_personality_v0(...)

; CHECK-LABEL: caught:
define void @caught() personality ptr @__gxx_personality_v0 {
entry:
  invoke void @may_throw() to label %done unwind label %lpad

; The reload is the first thing in the pad, and it comes after the label so the
; unwinder does not land past it.
; CHECK:      %lpad
; CHECK:      .Ltmp[[LP:[0-9]+]]:
; CHECK-NEXT: br $29, 1f
; CHECK-NEXT: 1: ldgp $29, 0($29)

; Nothing reaches the GOT before the reload has run.
; CHECK-NOT:  ($29)
; CHECK:      ldq $27, use($29)
lpad:
  %e = landingpad { ptr, i32 } cleanup
  %p = extractvalue { ptr, i32 } %e, 0
  call void @use(ptr %p)
  resume { ptr, i32 } %e

done:
  ret void
}

; The call-site table has to name that same label: if the reload were inserted
; ahead of it, the unwinder would jump straight past it as it does today.
; CHECK: .uleb128 .Ltmp[[LP]]-.Lfunc_begin0

; -msmall-text turns the calls into branches, but a landing pad is not reached by
; one: the unwinder enters it holding its own global pointer, so the reload is
; needed there just the same.
; SMALLTEXT-LABEL: caught:
; SMALLTEXT:      br $29, 1f
; SMALLTEXT-NEXT: 1: ldgp $29, 0($29)

; The pseudo needs an encoder of its own, so check object emission too: it is
; what every real compile goes through, and assembly output cannot catch its
; absence.  The branch is checked as bytes because only `br $31` is decodable --
; BRr, the form with an explicit link register, is isAsmParserOnly.
; OBJ:      00 00 a0 c3
; OBJ-NEXT: 00 00 bd 27 	ldah $29, 0($29)
; OBJ-NEXT: R_ALPHA_GPDISP	*ABS*+0x4
; OBJ-NEXT: 00 00 bd 23 	lda $29, 0($29)
