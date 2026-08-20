; Not a test by itself: the musttail call that Alpha cannot lower, driven by
; musttail.ll.  Nine integer arguments overflow $16-$21, so the call needs
; outgoing stack space inside the frame the tail jump has to tear down.
target triple = "alpha-unknown-linux-gnu"

declare dso_local i64 @ext9(i64, i64, i64, i64, i64, i64, i64, i64, i64)

define i64 @f9(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f, i64 %g, i64 %h,
               i64 %i) {
  %r = musttail call i64 @ext9(i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f,
                               i64 %g, i64 %h, i64 %i)
  ret i64 %r
}
