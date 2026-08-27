; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=2 \
; RUN:   -pass-remarks-missed=gisel-legalize -O2 < %s -o /dev/null 2>&1 \
; RUN:   | FileCheck %s
; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=2 \
; RUN:   -O0 < %s -o /dev/null

; Assembling a vector cannot survive scalarization -- if an element is still
; needed as a vector afterwards there is nowhere to put it -- so G_BUILD_VECTOR
; is reported rather than mislowered.  It must be *reported*: asking to
; scalarize it first aborted the compiler instead, because scalarize(0) hands
; fewerElementsVectorMerge a scalar narrow type and its first line asserts the
; narrow type is a vector.
;
; A vector argument is what reaches it.  Each element arrives in a whole
; register, is assembled with G_BUILD_VECTOR <N x s64> and truncated to the
; argument type; when the argument's only use is an extension there is no trunc
; for the artifact combiner to fold, and the build_vector reaches the rules.

; CHECK: Instruction selection used fallback path for zext_v2i8
define <2 x i32> @zext_v2i8(<2 x i8> %a) {
  %r = zext <2 x i8> %a to <2 x i32>
  ret <2 x i32> %r
}

; CHECK: Instruction selection used fallback path for sext_v4i8
define <4 x i32> @sext_v4i8(<4 x i8> %a) {
  %r = sext <4 x i8> %a to <4 x i32>
  ret <4 x i32> %r
}

; CHECK: Instruction selection used fallback path for zext_v8i8
define <8 x i16> @zext_v8i8(<8 x i8> %a) {
  %r = zext <8 x i8> %a to <8 x i16>
  ret <8 x i16> %r
}
