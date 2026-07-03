; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s

; A symbol the linker resolves itself sits at a fixed distance from the global
; pointer, so its address is formed with an ldah/lda pair rather than loaded
; from the GOT.  This is what keeps the GOT small: one gp reaches 64KB of it,
; and a large link has far more local symbols than that many entries.

@local = dso_local global i64 0
@priv = private unnamed_addr constant [4 x i8] c"abc\00"

; CHECK-LABEL: addr_local:
; CHECK:       ldah $0, local($29){{.*}}!gprelhigh
; CHECK-NEXT:  lda $0, local($0){{.*}}!gprellow
define ptr @addr_local() {
  ret ptr @local
}

; CHECK-LABEL: addr_private:
; CHECK:       ldah $0, .Lpriv($29){{.*}}!gprelhigh
; CHECK-NEXT:  lda $0, .Lpriv($0){{.*}}!gprellow
define ptr @addr_private() {
  ret ptr @priv
}

; A preemptible symbol's address is whatever the dynamic linker picks, so it
; still comes out of the GOT.

@ext = external global i64

; CHECK-LABEL: addr_preemptible:
; CHECK:       ldq $0, ext($29){{.*}}!literal
define ptr @addr_preemptible() {
  ret ptr @ext
}

; An undefined weak symbol has to read as zero, which only the GOT entry can
; deliver -- a gp-relative address is never null.

@wk = extern_weak dso_local global i64

; CHECK-LABEL: addr_weak:
; CHECK:       ldq $0, wk($29){{.*}}!literal
define ptr @addr_weak() {
  ret ptr @wk
}

; An absolute symbol is not in the data segment and has no offset from gp.

@abs = external dso_local global i64, !absolute_symbol !0

; CHECK-LABEL: addr_absolute:
; CHECK:       ldq $0, abs($29){{.*}}!literal
define ptr @addr_absolute() {
  ret ptr @abs
}

; An ifunc's address is the one its resolver returned, which the GOT entry
; holds; the symbol itself is only the resolver.

@ifn = dso_local ifunc i64 (), ptr @resolver

; CHECK-LABEL: addr_ifunc:
; CHECK:       ldq $0, ifn($29){{.*}}!literal
define ptr @addr_ifunc() {
  ret ptr @ifn
}

define internal ptr @resolver() {
  ret ptr null
}

!0 = !{i64 0, i64 256}
