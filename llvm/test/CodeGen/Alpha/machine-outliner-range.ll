; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s
; RUN: not llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 \
; RUN:   -filetype=obj < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=OBJ

; The outliner runs after branch relaxation -- Alpha adds BranchRelaxationPassID
; in addPreEmitPass(), and TargetPassConfig runs the outliner after it -- so the
; bsr it emits is never relaxed.  A bsr has a 21-bit displacement, +/-4 MiB, and
; the outlined function is appended after every function that calls it.  Nothing
; here is Alpha-specific: AArch64 and RISC-V order the two passes the same way.
;
; What makes that survivable is that the displacement is checked.  This pins
; that.  8 MiB of padding separates the first caller from the outlined function,
; so the call cannot be encoded, and the assembler says so rather than emitting
; a bsr to whatever the truncated displacement lands on.  A silent truncation
; here would be a wrong branch in correct-looking code; an error is a build that
; fails.  If this test ever stops failing to assemble, check that it is because
; the call is relaxed and not because the range check was lost.

; CHECK-LABEL: outline_a:
; CHECK: bsr $23, OUTLINED_FUNCTION_0
; OBJ: error: branch target out of range

define i64 @outline_a(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}

; The padding is emitted as one .space directive, so llc reaches the fixup and
; fails there without ever writing eight megabytes.
define void @pad() {
  call void asm sideeffect ".space 8388608, 0", ""()
  ret void
}

define i64 @outline_b(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}

define i64 @outline_c(i64 %a, i64 %b) minsize {
  %x = add i64 %a, %b
  %y = xor i64 %x, %a
  %z = and i64 %y, %b
  %w = mul i64 %z, %x
  %v = sub i64 %w, %y
  ret i64 %v
}
