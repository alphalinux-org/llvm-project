; RUN: llc -mtriple=alpha-unknown-linux-gnu -global-isel -global-isel-abort=1 < %s | FileCheck %s

; The three things that read or move the stack pointer at run time: the two
; halves of a va_list and a variable-length allocation.
;
; A va_list here is gcc's alpha_build_builtin_va_list, { char *base; int
; offset; }.  Slot N of the argument list lives at base + N*8 for every N, so
; the six register slots are a save area the callee writes on entry and slot 6
; onwards are the caller's stack arguments; that is why the save area sits at a
; fixed distance below the incoming stack pointer and not after the named
; arguments.

declare void @llvm.va_start.p0(ptr)
declare void @llvm.va_copy.p0(ptr, ptr)
declare void @use(ptr)

; One named argument, so $16 is spoken for and the save area is filled from
; $17 onwards -- into the slot for argument 1, not the slot for argument 0.
; The floating-point registers are saved alongside because which of the two
; areas a va_arg reads depends on the type it is given, which is not known
; here.
;
; CHECK-LABEL: start:
; CHECK:       lda $[[I:[0-9]+]], 80($30)
; CHECK:       stq $17, 8($[[I]])
; CHECK:       lda $[[F:[0-9]+]], 32($30)
; CHECK:       stt $f17, 8($[[F]])
; CHECK:       stq $21, 40($[[I]])
; CHECK:       stt $f21, 40($[[F]])
; The base is a pointer and is stored whole; the offset is an int, so it is
; stored four bytes wide rather than scribbling on the tail padding.
; CHECK:       stq $[[I]], 0($[[L:[0-9]+]])
; CHECK:       lda $[[O:[0-9]+]], 8($31)
; CHECK:       stl $[[O]], 8($[[L]])
define void @start(i64 %n, ...) {
  %l = alloca [1 x { ptr, i32 }]
  call void @llvm.va_start.p0(ptr %l)
  call void @use(ptr %l)
  ret void
}

; Both fields have to be copied.  Copying the pointer alone would leave the
; copy pointing at the start of the argument list however much of it the
; original had already consumed.
;
; CHECK-LABEL: copy:
; CHECK:       ldq $[[B:[0-9]+]], 0($16)
; CHECK:       ldl $[[C:[0-9]+]], 8($16)
; CHECK:       stq $[[B]], 0($[[D:[0-9]+]])
; CHECK:       stl ${{[0-9]+}}, 8($[[D]])
define void @copy(ptr %src) {
  %l = alloca [1 x { ptr, i32 }]
  call void @llvm.va_copy.p0(ptr %l, ptr %src)
  call void @use(ptr %l)
  ret void
}

; The stack grows down and is sixteen-byte aligned, which is the whole of what
; the generic lowering needs: round up, subtract, write back.
;
; CHECK-LABEL: vla:
; CHECK:       addq $[[S:[0-9]+]], 15, $[[S]]
; Rounding down is a bic: the mask is the complement of a literal, which is
; the operand form bic takes.
; CHECK:       bic $[[S]], 15, $[[S]]
; CHECK:       subq $30, $[[S]], $[[P:[0-9]+]]
; CHECK:       bis $31, $[[P]], $30
define void @vla(i64 %n) {
  %p = alloca i8, i64 %n
  call void @use(ptr %p)
  ret void
}
