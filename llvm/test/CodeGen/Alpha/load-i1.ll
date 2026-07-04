; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=ev6 < %s | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s \
; RUN:   --check-prefix=NOBWX

; The promotion is to a byte load either way; what changes is how a byte is
; loaded.  Without BWX -- the default, which -mcpu=ev6 is the exception to --
; that is the aligned quadword around it and an extract, and nothing else
; covered the i1 case on that path.
; CHECK-LABEL: load_bool:
; CHECK:       ldbu {{\$[0-9]+}}, 0($16)
; NOBWX-LABEL: load_bool:
; NOBWX:       ldq_u {{\$[0-9]+}}, 0($16)
; NOBWX:       extbl
define zeroext i1 @load_bool(ptr %p) {
  %v = load i1, ptr %p
  ret i1 %v
}

; CHECK-LABEL: use_bool:
; CHECK:       ldbu
; NOBWX-LABEL: use_bool:
; NOBWX:       extbl
define i64 @use_bool(ptr %p) {
  %b = load i1, ptr %p
  %r = select i1 %b, i64 10, i64 20
  ret i64 %r
}
