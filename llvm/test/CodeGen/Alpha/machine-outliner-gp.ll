; RUN: llc -verify-machineinstrs -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s \
; RUN:   | FileCheck %s

; A bsr to an outlined function leaves the caller's global pointer alone: the
; outlined body cannot touch $29 -- getOutliningTypeImpl rules out every
; instruction that reads or writes it -- and it returns through $23 without an
; ldgp.  So an access to a global after the call reads the $29 the prologue
; established, and no reload appears between them.  That is also why the B4
; invariant (AlphaVerifyInvariants) must not treat a BSR as clobbering it:
; reading a global after an outlined call is all it takes to make a check that
; does refuse a correct function.
;
; This is a file of its own because the outlined functions are numbered in the
; order they are created, so adding a sequence to machine-outliner.ll renumbers
; the ones already being checked there.

@gv = external hidden global i64

; CHECK-LABEL: gp_after_outlined_a:
; CHECK:       ldgp $29, 0($27)
; CHECK:       bsr $23, OUTLINED_FUNCTION_0
; CHECK-NOT:   ldgp
; CHECK:       ldah $1, gv($29){{.*}}!gprelhigh

define i64 @gp_after_outlined_a(i64 %a, i64 %b) minsize {
  %x = or i64 %a, %b
  %y = xor i64 %x, 12345
  %z = mul i64 %y, %x
  %w = sub i64 %z, %y
  %v = and i64 %w, %z
  %g = load i64, ptr @gv
  %r = add i64 %v, %g
  ret i64 %r
}

define i64 @gp_after_outlined_b(i64 %a, i64 %b) minsize {
  %x = or i64 %a, %b
  %y = xor i64 %x, 12345
  %z = mul i64 %y, %x
  %w = sub i64 %z, %y
  %v = and i64 %w, %z
  %g = load i64, ptr @gv
  %r = add i64 %v, %g
  ret i64 %r
}

define i64 @gp_after_outlined_c(i64 %a, i64 %b) minsize {
  %x = or i64 %a, %b
  %y = xor i64 %x, 12345
  %z = mul i64 %y, %x
  %w = sub i64 %z, %y
  %v = and i64 %w, %z
  %g = load i64, ptr @gv
  %r = add i64 %v, %g
  ret i64 %r
}

; CHECK-LABEL: OUTLINED_FUNCTION_0:
; CHECK-NOT:   $29
; CHECK:       jmp $31, ($23), 0
