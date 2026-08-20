; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -O2 -filetype=obj < %s \
; RUN:   | llvm-readobj -r - | FileCheck --check-prefix=RELOC %s

; A direct call loads its own procedure value even when the same callee was
; already loaded a few instructions earlier. Sharing one load would be shorter,
; but a linker may only delete a load whose every use is a call it has just
; turned into a direct branch, and it can only tell which uses those are from
; the R_ALPHA_LITUSE that follows each R_ALPHA_LITERAL. gcc reloads for the same
; reason.

declare i32 @callee(i32)

; CHECK-LABEL: two_calls:
; CHECK:      ldq $27, callee($29)
; CHECK:      jsr $26, ($27)
; CHECK:      ldq $27, callee($29)
; CHECK:      jsr $26, ($27)

; Each literal is followed by its own lituse, which is the pairing the linker
; relies on.
; RELOC:      R_ALPHA_LITERAL callee
; RELOC-NEXT: R_ALPHA_LITUSE - 0x3
; RELOC-NEXT: R_ALPHA_HINT callee
; RELOC:      R_ALPHA_LITERAL callee
; RELOC-NEXT: R_ALPHA_LITUSE - 0x3
; RELOC-NEXT: R_ALPHA_HINT callee
define i32 @two_calls(i32 %x) {
  %a = call i32 @callee(i32 %x)
  %b = call i32 @callee(i32 %a)
  ret i32 %b
}

; The lituse relocation carries only its use type, so it must not name a
; section the instruction is not in.
; RELOC-LABEL: Section {{.*}} .rela.mysection {
; RELOC: R_ALPHA_LITUSE - 0x3
define void @in_its_own_section() section ".mysection" {
  %r = call i32 @callee(i32 0)
  ret void
}
