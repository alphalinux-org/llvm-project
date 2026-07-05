; RUN: opt -mtriple=alpha-unknown-linux-gnu -mattr=+cix -passes='loop(loop-idiom)' \
; RUN:   -S < %s | FileCheck %s --check-prefix=CIX
; RUN: opt -mtriple=alpha-unknown-linux-gnu -mattr=-cix -passes='loop(loop-idiom)' \
; RUN:   -S < %s | FileCheck %s --check-prefix=NOCIX

; LoopIdiomRecognize replaces a count-the-set-bits loop with llvm.ctpop only
; where the target reports the population count as fast hardware, which is what
; AlphaTTIImpl::getPopcntSupport answers for: ctpop is one CIX instruction, and
; without CIX it expands to a bit-twiddling sequence that is no better than the
; loop.

; CIX-LABEL: @popcount(
; CIX: call i64 @llvm.ctpop.i64
; NOCIX-LABEL: @popcount(
; NOCIX-NOT: llvm.ctpop
define i32 @popcount(i64 %x) {
entry:
  %tobool3 = icmp eq i64 %x, 0
  br i1 %tobool3, label %done, label %loop

loop:
  %n.05 = phi i64 [ %and, %loop ], [ %x, %entry ]
  %c.04 = phi i32 [ %inc, %loop ], [ 0, %entry ]
  %sub = add i64 %n.05, -1
  %and = and i64 %sub, %n.05
  %inc = add nsw i32 %c.04, 1
  %tobool = icmp eq i64 %and, 0
  br i1 %tobool, label %done, label %loop

done:
  %c.0.lcssa = phi i32 [ 0, %entry ], [ %inc, %loop ]
  ret i32 %c.0.lcssa
}
