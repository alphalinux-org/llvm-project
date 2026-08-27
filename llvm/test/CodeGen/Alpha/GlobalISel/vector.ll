; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 -O2 < %s | FileCheck %s

; There are no vector registers, so every vector operation is scalarized: a
; four-element add is four adds, and a vector load is four loads.  This is what
; the SelectionDAG path does with the same input, and it is the only thing that
; can be done -- but the legalizer had no vector rules at all, so any function
; touching a vector fell back to that path whole.

; CHECK-LABEL: vadd:
; CHECK-COUNT-8: ldl $
; CHECK-COUNT-4: addq $
; CHECK-COUNT-4: stl $
define void @vadd(ptr %p, ptr %q) {
  %a = load <4 x i32>, ptr %p
  %b = load <4 x i32>, ptr %q
  %r = add <4 x i32> %a, %b
  store <4 x i32> %r, ptr %p
  ret void
}

; CHECK-LABEL: vfadd:
; CHECK-COUNT-4: ldt $f
; CHECK-COUNT-2: addt $f
; CHECK-COUNT-2: stt $f
define void @vfadd(ptr %p, ptr %q) {
  %a = load <2 x double>, ptr %p
  %b = load <2 x double>, ptr %q
  %r = fadd <2 x double> %a, %b
  store <2 x double> %r, ptr %p
  ret void
}

; A comparison and a select scalarize together: the element-wide condition
; becomes one condition per element.
; CHECK-LABEL: vsel:
; CHECK-COUNT-4: cmplt $
; CHECK-COUNT-4: cmovne $
define void @vsel(ptr %p, ptr %q) {
  %a = load <4 x i32>, ptr %p
  %b = load <4 x i32>, ptr %q
  %c = icmp slt <4 x i32> %a, %b
  %r = select <4 x i1> %c, <4 x i32> %a, <4 x i32> %b
  store <4 x i32> %r, ptr %p
  ret void
}

; Reading one element out of a vector is reading one element out of memory once
; the vector itself is in pieces; nothing is built and taken apart again.
; CHECK-LABEL: vext:
; CHECK:       ldl $0, 12($16)
; CHECK-NEXT:  ret
define i32 @vext(ptr %p) {
  %a = load <4 x i32>, ptr %p
  %r = extractelement <4 x i32> %a, i32 3
  ret i32 %r
}

; An element index that is not a constant cannot pick a register, so the vector
; goes through a stack slot and the element is loaded from a computed address.
; CHECK-LABEL: vextv:
; CHECK:       lda $30, -
; CHECK:       ldl $0,
define i32 @vextv(ptr %p, i32 %i) {
  %a = load <4 x i32>, ptr %p
  %r = extractelement <4 x i32> %a, i32 %i
  ret i32 %r
}

; CHECK-LABEL: vins:
; CHECK:       lda $[[V:[0-9]+]], 99($31)
; CHECK:       stl $[[V]], 4($16)
define void @vins(ptr %p, i32 %x) {
  %a = load <4 x i32>, ptr %p
  %r = insertelement <4 x i32> %a, i32 99, i32 1
  store <4 x i32> %r, ptr %p
  ret void
}

; A shuffle with a constant mask is a permutation of the pieces, so it costs
; nothing beyond the loads and stores.
; CHECK-LABEL: vshuf:
; CHECK-NOT:   $30
; CHECK:       ret
define void @vshuf(ptr %p, ptr %q) {
  %a = load <4 x i32>, ptr %p
  %b = load <4 x i32>, ptr %q
  %r = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32> <i32 7, i32 2, i32 5, i32 0>
  store <4 x i32> %r, ptr %p
  ret void
}

; A vector with no defining computation is scalarized like any other, into one
; undefined element each.  Without a rule for it the legalizer gave up here,
; which took every function containing a partly-undefined vector -- what an
; insertelement into undef starts as -- back to the SelectionDAG path.
; CHECK-LABEL: vundef:
; CHECK:       ret
define <4 x i32> @vundef() {
  ret <4 x i32> undef
}

; An insertelement into undef is the ordinary way a vector is built, and it is
; the case that reaches G_IMPLICIT_DEF with the other three elements left
; undefined.  Only the inserted element survives to the extract, so the answer
; is the argument itself.
; CHECK-LABEL: vundef_insert:
; CHECK:       bis $31, $16, $0
define i32 @vundef_insert(i32 %x) {
  %v = insertelement <4 x i32> undef, i32 %x, i32 0
  %e = extractelement <4 x i32> %v, i32 0
  ret i32 %e
}
