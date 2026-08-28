; GCC's alpha_cpu_table gives most processors a part-number spelling as well as
; an EV name, and those are what a build system older than the EV names passes.
; Each one selects the same processor as its EV twin, so it enables the same
; extensions -- the .arch directive names the feature set, so it is what the
; two spellings have to agree on -- and none of them draws "not a recognized
; processor" from the subtarget.

; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21064 %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21164 %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=EV4
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21164a %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=EV56
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21164pc %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=PCA56
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21164PC %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=PCA56
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21264 %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefix=EV6
; RUN: llc -mtriple=alpha-unknown-linux-gnu -mcpu=21264a %s -o - 2>&1 \
; RUN:   | FileCheck %s --check-prefixes=EV6,EV67

; EV4-NOT:   not a recognized processor
; EV4:       .arch ev4
; EV56-NOT:  not a recognized processor
; EV56:      .arch ev56
; PCA56-NOT: not a recognized processor
; PCA56:     .arch pca56
; EV6-NOT:   not a recognized processor
; EV6:       .arch ev6

; .arch does not separate ev6 from ev67, so the one feature that does -- CIX,
; which turns a popcount into a single ctpop -- says which of the two 21264
; spellings picked up the extra extension.
; EV6:       popcount:
; EV6-NOT:   ctpop
; EV67:      ctpop
define void @f() { ret void }

declare i64 @llvm.ctpop.i64(i64)
define i64 @popcount(i64 %x) {
  %r = call i64 @llvm.ctpop.i64(i64 %x)
  ret i64 %r
}
