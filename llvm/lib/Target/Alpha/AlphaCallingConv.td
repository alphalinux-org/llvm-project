//===-- AlphaCallingConv.td - Alpha calling conventions ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Alpha OSF/ELF ABI.
//
//===----------------------------------------------------------------------===//

// Integer args in $16-$21, FP args in $f16-$f21, remaining args on the stack
// in 8-byte slots.  The six argument slots are shared between the integer and
// floating registers: the Nth argument uses $16+N or $f16+N by its type, so an
// integer register and its FP counterpart are allocated together (via a shadow)
// to keep the slot index in step regardless of the mix of types.
def CC_Alpha : CallingConv<[
  CCIfType<[i8, i16, i32], CCPromoteToType<i64>>,
  CCIfType<[i64], CCAssignToRegWithShadow<[R16, R17, R18, R19, R20, R21],
                                          [F16, F17, F18, F19, F20, F21]>>,
  CCIfType<[f32, f64], CCAssignToRegWithShadow<[F16, F17, F18, F19, F20, F21],
                                               [R16, R17, R18, R19, R20, R21]>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>,
  CCIfType<[f32, f64], CCAssignToStack<8, 8>>
]>;

// Return values: an integer in $0, a floating-point value in $f0, and the two
// halves of a complex float or complex double in $f0/$f1.  Anything larger than
// a register -- an aggregate, __int128, X_floating -- is returned in memory
// through a hidden pointer the caller passes in $16, so only one integer
// register is available here; see AlphaTargetLowering::CanLowerReturn.
def RetCC_Alpha : CallingConv<[
  CCIfType<[i8, i16, i32], CCPromoteToType<i64>>,
  CCIfType<[i64], CCAssignToReg<[R0]>>,
  CCIfType<[f32, f64], CCAssignToReg<[F0, F1]>>
]>;

// Registers preserved across a function (callee-saved): $9-$15, $f2-$f9, and
// the return address $26, which a function that makes calls must save and
// restore because jsr clobbers it.  $15 is the frame pointer when needed.
def CSR_Alpha : CalleeSavedRegs<(add R9, R10, R11, R12, R13, R14, R15,
                                     F2, F3, F4, F5, F6, F7, F8, F9, R26)>;

// Registers preserved across a call.  A callee does not preserve $26 (jsr
// overwrites it with the return address), so it is excluded here.
def CSR_Alpha_Call : CalleeSavedRegs<(sub CSR_Alpha, R26)>;
