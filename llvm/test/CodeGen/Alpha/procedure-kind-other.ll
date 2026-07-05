; RUN: llc -mtriple=alpha-unknown-linux-gnu -filetype=obj < %s -o %t.o
; RUN: llvm-readelf -s %t.o | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu < %s | FileCheck %s --check-prefix=ASM

; Each function records its procedure kind in st_other, as GNU as does from
; .ent/.prologue: one that establishes its own global pointer advertises the
; standard gp load (0x88), one that runs on the caller's gp needs no procedure
; value (0x80).

; CHECK: [<other: 0x88>] {{[0-9]+}} uses_gp
; CHECK: [<other: 0x80>] {{[0-9]+}} no_gp

; The textual form carries the same fact in the .ent/.prologue/.end an external
; assembler reads: .prologue 1 for the standard gp load, .prologue 0 for a
; function that needs no procedure value.
; ASM:      .ent uses_gp
; ASM:      .prologue 1
; ASM:      .end uses_gp
; ASM:      .ent no_gp
; ASM:      .prologue 0
; ASM:      .end no_gp

@g = external global i64

define i64 @uses_gp() {
  %v = load i64, ptr @g
  ret i64 %v
}

define i64 @no_gp(i64 %x) {
  %r = add i64 %x, 1
  ret i64 %r
}
