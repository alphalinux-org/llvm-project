; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-text < %s | FileCheck %s --check-prefix=SMALL
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-text -filetype=obj < %s \
; RUN:   | llvm-objdump -dr - | FileCheck %s --check-prefix=OBJ

; With -msmall-text a direct call is a single PC-relative branch: no procedure
; value is loaded through the GOT, the callee is not reached via jsr, and the
; caller does not reload the global pointer afterwards.  The branch takes the
; R_ALPHA_BRSGP relocation, which is what lets the callee run on the caller's
; global pointer: the linker aims it past the callee's own gp prologue.

declare dso_local i32 @g(i32)

; The caller still establishes its own global pointer.  It has to: the branch it
; makes skips the callee's prologue, so the callee inherits this $29, and a
; caller of ours in another gp region -- libc entering main, or calling back
; into a function whose address we gave it -- leaves $29 holding theirs.

; LARGE-LABEL: f:
; LARGE: ldgp $29, 0($27)
; LARGE: ldq $27, g($29)
; LARGE: jsr $26, ($27)
; LARGE: ldgp $29, 0($26)

; SMALL-LABEL: f:
; SMALL: ldgp $29, 0($27)
; SMALL-NOT: !literal
; SMALL: bsr $26, g		!samegp
; SMALL-NOT: jsr
define i32 @f(i32 %x) {
  %r = call i32 @g(i32 %x)
  ret i32 %r
}

; A tail call is the same: the callee is reached with a br, which like jmp
; discards the return address, and there is no procedure value to load.

define dso_local i32 @h(i32 %x) {
  ret i32 %x
}

; LARGE-LABEL: tail:
; h is defined here, so it runs on our gp and the tail call stands.
; LARGE: ldq $27, h($29){{.*}}!literal
; LARGE: jmp $31, ($27), 0

; SMALL-LABEL: tail:
; SMALL: ldgp $29, 0($27)
; SMALL-NOT: !literal
; SMALL: br $31, h		!samegp
; SMALL-NOT: jmp
define i32 @tail(i32 %x) {
  %r = tail call i32 @h(i32 %x)
  ret i32 %r
}

; The branch is a bsr the linker aims past the callee's own ldgp, so under
; -msmall-text even a callee defined in another translation unit runs on our
; gp -- and the tail call stands where the large model has to refuse it.  This
; is the -msmall-text half of gcc's decl_has_samegp.
; LARGE-LABEL: tail_extern:
; LARGE-NOT: br $31, g
; LARGE: ldq $27, g($29)
; LARGE: jsr $26, ($27)
; LARGE: ldgp $29, 0($26)

; SMALL-LABEL: tail_extern:
; SMALL-NOT: !literal
; SMALL: br $31, g		!samegp
; SMALL-NOT: jmp
define i32 @tail_extern(i32 %x) {
  %r = tail call i32 @g(i32 %x)
  ret i32 %r
}

; A preemptible callee keeps the GOT sequence.  Its address is the dynamic
; linker's to pick, and a pc-relative relocation against a dynamic symbol is not
; something the static linker can leave behind:
;
;   ld: pc-relative relocation against dynamic symbol perror@@GLIBC_2.0

declare i32 @preemptible(i32)

; SMALL-LABEL: interposable:
; SMALL: ldq $27, preemptible($29)
; SMALL: jsr $26, ($27)
; SMALL-NOT: !samegp
define i32 @interposable(i32 %x) {
  %r = call i32 @preemptible(i32 %x)
  ret i32 %r
}

; A preemptible callee cannot be tail-called at all: it resolves into another
; DSO, and its gp would be the one our caller found in $29 on return.
; SMALL-LABEL: interposable_tail:
; SMALL: ldq $27, preemptible($29)
; SMALL-NOT: jmp $31, ($27), 0
; SMALL: jsr $26, ($27)
; SMALL-NOT: !samegp
define i32 @interposable_tail(i32 %x) {
  %r = tail call i32 @preemptible(i32 %x)
  ret i32 %r
}

; The relocation is the point of the !samegp suffix, so check the object file:
; assembly output alone would not catch a branch emitted as a plain BRADDR.

; OBJ-LABEL: <f>:
; OBJ: R_ALPHA_BRSGP g
; OBJ-LABEL: <tail>:
; OBJ: R_ALPHA_BRSGP h
; OBJ-LABEL: <interposable>:
; OBJ: R_ALPHA_LITERAL preemptible

; The branch runs the callee on our global pointer and there is no reload after
; it, so $29 still holds ours when the next access to a global reads it -- which
; is also why the B4 invariant (AlphaVerifyInvariants) must not treat a CALLbsr
; as clobbering it.  A call followed by any access to a global is enough to make
; it fire.

@gv = external hidden global i32

; LARGE-LABEL: call_then_gp:
; LARGE: jsr $26, ($27)
; LARGE: ldgp $29, 0($26)
; LARGE: ldah $1, gv($29)

; SMALL-LABEL: call_then_gp:
; SMALL: ldgp $29, 0($27)
; SMALL: bsr $26, g		!samegp
; SMALL-NOT: ldgp
; SMALL: ldah $1, gv($29)
define i32 @call_then_gp(i32 %x) {
  %r = call i32 @g(i32 %x)
  %v = load i32, ptr @gv
  %s = add i32 %r, %v
  ret i32 %s
}
