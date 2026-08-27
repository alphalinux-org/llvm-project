; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; Inline asm through GlobalISel.  Without a target inline asm lowering the
; IRTranslator cannot translate the call at all, so every function containing
; an asm statement -- most of a kernel or a C library -- fell back to the
; SelectionDAG path.  The constant-integer constraints are the Alpha letters,
; whose ranges the generic lowering does not know either.

define i64 @reg(i64 %a, i64 %b) {
; CHECK-LABEL: reg:
; CHECK: addq $16, $17, $0
  %r = call i64 asm "addq $1,$2,$0", "=r,r,r"(i64 %a, i64 %b)
  ret i64 %r
}

; "I" is an unsigned 8-bit literal, which the operate format takes directly.
define i64 @imm_I(i64 %a) {
; CHECK-LABEL: imm_I:
; CHECK: addq $16, 200, $0
  %r = call i64 asm "addq $1,$2,$0", "=r,r,I"(i64 %a, i64 200)
  ret i64 %r
}

; "K" is a signed 16-bit displacement, which lda takes.
define i64 @imm_K(i64 %a) {
; CHECK-LABEL: imm_K:
; CHECK: lda $0, -30000($16)
  %r = call i64 asm "lda $0,$1($2)", "=r,K,r"(i64 -30000, i64 %a)
  ret i64 %r
}

; Out of range for "I", so the register alternative of "rI" is used instead.
define i64 @imm_I_wide(i64 %a) {
; CHECK-LABEL: imm_I_wide:
; CHECK: lda [[T:\$[0-9]+]], 1000($31)
; CHECK: addq $16, [[T]], $0
  %r = call i64 asm "addq $1,$2,$0", "=r,r,rI"(i64 %a, i64 1000)
  ret i64 %r
}

; A "f" operand is given F4RC or F8RC, which hold the same registers as FPRC
; but are not subclasses of it -- asking FPRC alone made the copy into the asm
; operand look like a move between the banks.
define double @fp(double %a, double %b) {
; CHECK-LABEL: fp:
; CHECK: addt $f16, $f17, $f0
  %r = call double asm "addt $1,$2,$0", "=f,f,f"(double %a, double %b)
  ret double %r
}

define float @fp32(float %a) {
; CHECK-LABEL: fp32:
; CHECK: adds $f16, $f16, $f0
; CHECK-NOT: stt
  %r = call float asm "adds $1,$1,$0", "=f,f"(float %a)
  ret float %r
}

@sym = external global i64

; "R" names a symbol directly rather than materializing its address.
define void @symbolic() {
; CHECK-LABEL: symbolic:
; CHECK: bsr $26, sym
  call void asm "bsr $$26,$0", "R"(ptr @sym)
  ret void
}

; An "m" operand.  The SelectionDAG path splits the address into a base and a
; displacement, so the operand group holds two operands; this path has no hook
; for that and passes the whole address in one register.  The asm printer used
; to read the second operand either way, which ran off the end of the
; instruction and asserted.
define i64 @mem(ptr %p) {
; CHECK-LABEL: mem:
; CHECK: ldq $0, 0($16)
  %r = call i64 asm "ldq $0, $1", "=r,*m"(ptr elementtype(i64) %p)
  ret i64 %r
}

; With a displacement the address is computed first, so the operand is still
; printed with a zero one.
define i64 @mem_disp(ptr %p) {
; CHECK-LABEL: mem_disp:
; CHECK: lda $0, 24($16)
; CHECK: ldq $0, 0($0)
  %q = getelementptr i64, ptr %p, i64 3
  %r = call i64 asm "ldq $0, $1", "=r,*m"(ptr elementtype(i64) %q)
  ret i64 %r
}

; An "=m" output, which is not the last operand group.  Reading past a
; one-operand group here found the next group's flag word rather than the end
; of the instruction, so the displacement came out as 262153 and only the
; assembler complained.
define void @mem_out(ptr %p, i64 %v) {
; CHECK-LABEL: mem_out:
; CHECK: stq $17, 0($16)
  call void asm "stq $1, $0", "=*m,r"(ptr elementtype(i64) %p, i64 %v)
  ret void
}

; "R" takes a block address as well, which is what the SelectionDAG path
; accepts.  An external symbol has no Value at this level, so that third case
; has no counterpart here.
define void @symbolic_block() {
; CHECK-LABEL: symbolic_block:
; CHECK: bsr $26, .Ltmp0
entry:
  call void asm "bsr $$26,$0", "R"(ptr blockaddress(@symbolic_block, %lbl))
  br label %lbl
lbl:
  ret void
}
