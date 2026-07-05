; RUN: opt -mtriple=alpha-unknown-linux-gnu -passes=consthoist -S < %s | FileCheck %s

; AlphaTTIImpl::getIntImmCost prices a constant by the instructions that build
; it: one lda for a signed 16-bit value, an ldah/lda pair for a 32-bit one, and
; four for anything wider.  ConstantHoisting reads that price, so a wide
; constant used three times is materialized once and reused, while a 16-bit one
; stays where it is -- rematerializing it is as cheap as keeping it live.

; CHECK-LABEL: @narrow(
; CHECK-NOT: %const
define i64 @narrow(i64 %a, i64 %b, i1 %c) {
entry:
  br i1 %c, label %x, label %y

x:
  %r1 = add i64 %a, 1234
  br label %z

y:
  %r2 = add i64 %b, 1234
  br label %z

z:
  %p = phi i64 [ %r1, %x ], [ %r2, %y ]
  %q = add i64 %p, 1234
  ret i64 %q
}

; CHECK-LABEL: @wide(
; CHECK: %const = bitcast i64 4822678189205111 to i64
; CHECK: add i64 %a, %const
; CHECK: add i64 %b, %const
; CHECK: add i64 %p, %const
define i64 @wide(i64 %a, i64 %b, i1 %c) {
entry:
  br i1 %c, label %x, label %y

x:
  %r1 = add i64 %a, 4822678189205111
  br label %z

y:
  %r2 = add i64 %b, 4822678189205111
  br label %z

z:
  %p = phi i64 [ %r1, %x ], [ %r2, %y ]
  %q = add i64 %p, 4822678189205111
  ret i64 %q
}
