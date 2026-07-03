; RUN: llc -mtriple=alpha-unknown-linux-gnu -relocation-model=static < %s | FileCheck %s

; Under -fno-pic clang marks every symbol dso_local, an `extern` declaration of
; something that in fact lives in a shared library included.  A gp-relative
; address promises the definition ends up in this link unit's gp region, and
; the linker cannot keep that promise for such a declaration: it resolves the
; function to a PLT entry and the data to a copy relocation, and
; R_ALPHA_GPREL{HIGH,LOW} names neither.  (`g++ -no-pie` against libstdc++'s
; typeinfo is the case that fails to link.)  So an external declaration with
; default visibility goes through the GOT even here, which is also what gcc
; does: its local_symbolic_operand asks SYMBOL_REF_LOCAL_P.

@ext = external global i64
@ext_dso = external dso_local global i64
@ext_hidden = external hidden global i64
@ext_protected = external protected global i64
@def = dso_local global i64 0

declare void @ext_fn()
declare dso_local void @ext_dso_fn()

; CHECK-LABEL: addr_ext:
; CHECK:       ldq $0, ext($29){{.*}}!literal
; CHECK-NOT:   !gprelhigh
define ptr @addr_ext() {
  ret ptr @ext
}

; The case clang actually emits: under -fno-pic every symbol is marked
; dso_local, so an `extern` declaration of something that in fact lives in a
; shared library arrives here as `external dso_local`.  This is the case the
; rule exists for -- @ext above is already rejected for not being dso_local at
; all, so it cannot tell whether the declaration test is present.
; CHECK-LABEL: addr_ext_dso:
; CHECK:       ldq $0, ext_dso($29){{.*}}!literal
; CHECK-NOT:   !gprelhigh
define ptr @addr_ext_dso() {
  ret ptr @ext_dso
}

; CHECK-LABEL: addr_ext_dso_fn:
; CHECK:       ldq $0, ext_dso_fn($29){{.*}}!literal
; CHECK-NOT:   !gprelhigh
define ptr @addr_ext_dso_fn() {
  ret ptr @ext_dso_fn
}

; Non-default visibility is the promise that the definition is ours.
; CHECK-LABEL: addr_ext_hidden:
; CHECK:       ldah $0, ext_hidden($29){{.*}}!gprelhigh
; CHECK-NEXT:  lda $0, ext_hidden($0){{.*}}!gprellow
define ptr @addr_ext_hidden() {
  ret ptr @ext_hidden
}

; CHECK-LABEL: addr_ext_protected:
; CHECK:       ldah $0, ext_protected($29){{.*}}!gprelhigh
define ptr @addr_ext_protected() {
  ret ptr @ext_protected
}

; A definition in this module is unaffected.
; CHECK-LABEL: addr_def:
; CHECK:       ldah $0, def($29){{.*}}!gprelhigh
define ptr @addr_def() {
  ret ptr @def
}

; The rule is about the symbol, not about it being data: taking the address of
; an external function has the same problem.
; CHECK-LABEL: addr_ext_fn:
; CHECK:       ldq $0, ext_fn($29){{.*}}!literal
; CHECK-NOT:   !gprelhigh
define ptr @addr_ext_fn() {
  ret ptr @ext_fn
}
