; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee-conformant < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=NONE
; The assembly this produces has to be assemblable by the integrated assembler,
; which is what the directive is for.
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+ieee-conformant < %s \
; RUN:   | llvm-mc -triple=alpha-unknown-linux-gnu -filetype=obj -o /dev/null

; -mieee-conformant marks the object as IEEE conformant with a .eflag 48 in
; each function prologue, which is what asks the loader to enable software
; completion.  gcc emits it from alpha_start_function under
; TARGET_IEEE_CONFORMANT and documents it as the option's only effect; gcc 16.2
; produces the same directive for the same source.
;
; It is textual only: an object file records the same thing through the
; assembler, so there is nothing to emit when the integrated assembler is
; encoding directly.

; CHECK-LABEL: f:
; CHECK:       .ent f
; CHECK:       .eflag 48
; CHECK:       .end f
; NONE-LABEL:  f:
; NONE-NOT:    .eflag
define void @f() { ret void }

; Per function, not once per file.
; CHECK-LABEL: g:
; CHECK:       .eflag 48
define void @g() { ret void }
