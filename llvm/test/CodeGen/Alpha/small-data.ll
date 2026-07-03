; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s \
; RUN:   | FileCheck %s --check-prefix=LARGE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+small-data < %s \
; RUN:   | FileCheck %s --check-prefix=SMALL

@g = global i64 42

; By default (large-data) a global's address is loaded from the GOT.
; LARGE-LABEL: get:
; LARGE:       ldq $0, g($29){{.*}}!literal
; LARGE:       ldq $0, 0($0)

; With small-data the address is formed GP-relative.
; SMALL-LABEL: get:
; SMALL:       ldah $0, g($29){{.*}}!gprelhigh
; SMALL:       lda $0, g($0){{.*}}!gprellow
; SMALL:       ldq $0, 0($0)
define i64 @get() {
  %v = load i64, ptr @g
  ret i64 %v
}
