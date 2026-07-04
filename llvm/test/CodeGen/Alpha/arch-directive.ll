; The assembler output carries a .arch directive covering the enabled features,
; so an external assembler accepts the extension instructions.  It is textual
; only -- the integrated assembler encodes directly.
; RUN: llc -mtriple=alpha-unknown-linux-gnu %s -o - | FileCheck %s --check-prefix=BASE
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+bwx %s -o - | FileCheck %s --check-prefix=BWX
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+bwx,+mvi %s -o - | FileCheck %s --check-prefix=MVI
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+fix %s -o - | FileCheck %s --check-prefix=EV6
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mattr=+cix %s -o - | FileCheck %s --check-prefix=EV6

; BASE: .arch ev4
; BWX:  .arch ev56
; MVI:  .arch pca56
; EV6:  .arch ev6
define void @f() { ret void }
