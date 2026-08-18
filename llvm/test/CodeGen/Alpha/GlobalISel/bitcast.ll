; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; A bitcast between an integer and a floating type is not an instruction on
; Alpha: there is no register-to-register move between the two files, so the
; value goes through memory.  The IR-level bitcast disappears before the
; register bank information ever sees it -- both ends have the same LLT, so
; the translator emits a plain COPY -- and it is the cross-bank COPY path in
; AlphaInstructionSelector::selectCopy that turns it into the move pseudo.
;
; The width matters.  A 64-bit value is a bit copy (stq/ldt, stt/ldq), but a
; 32-bit one has to go through the S_floating memory format, because a float
; held in a register is in T_floating form and the four bytes a bitcast names
; are the S_floating ones.  That is what MOVi2f_S and MOVf2i_S are for, and
; getting it wrong would silently change the bits.

; CHECK-LABEL: bitcast_i64_to_double:
; CHECK:      stq $16, {{[0-9]+}}($30)
; CHECK:      ldt {{\$f[0-9]+}}, {{[0-9]+}}($30)
define double @bitcast_i64_to_double(i64 %x) {
  %r = bitcast i64 %x to double
  %s = fadd double %r, %r
  ret double %s
}

; CHECK-LABEL: bitcast_double_to_i64:
; CHECK:      stt $f16, {{[0-9]+}}($30)
; CHECK:      ldq {{\$[0-9]+}}, {{[0-9]+}}($30)
define i64 @bitcast_double_to_i64(double %x) {
  %r = bitcast double %x to i64
  %s = add i64 %r, %r
  ret i64 %s
}

; stl/lds, not stq/ldt: the load converts S_floating to the T_floating form
; the register holds.
; CHECK-LABEL: bitcast_i32_to_float:
; CHECK:      stl $16, {{[0-9]+}}($30)
; CHECK:      lds {{\$f[0-9]+}}, {{[0-9]+}}($30)
; CHECK-NOT:  ldt
define float @bitcast_i32_to_float(i32 %x) {
  %r = bitcast i32 %x to float
  %s = fadd float %r, %r
  ret float %s
}

; CHECK-LABEL: bitcast_float_to_i32:
; CHECK:      sts $f16, {{[0-9]+}}($30)
; CHECK:      ldl {{\$[0-9]+}}, {{[0-9]+}}($30)
; CHECK-NOT:  stt
define i32 @bitcast_float_to_i32(float %x) {
  %r = bitcast float %x to i32
  %s = add i32 %r, %r
  ret i32 %s
}

; A freeze of a floating value belongs in the floating bank and costs nothing.
; CHECK-LABEL: freeze_fp:
; CHECK-NOT:  ($30)
; CHECK:      addt $f16, $f16, $f0
define double @freeze_fp(double %x) {
  %r = freeze double %x
  %s = fadd double %r, %r
  ret double %s
}

; The frozen value has no computation behind it, so only the use decides the
; bank.  The result still has to reach a floating register.
; CHECK-LABEL: freeze_undef_fp:
; CHECK:      ldt [[F:\$f[0-9]+]], {{[0-9]+}}($30)
; CHECK:      addt [[F]], [[F]], $f0
define double @freeze_undef_fp() {
  %r = freeze double undef
  %s = fadd double %r, %r
  ret double %s
}
