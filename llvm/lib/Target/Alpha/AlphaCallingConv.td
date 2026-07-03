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

// Integer args in $16-$21, FP args in $f16-$f21 (register slots are shared),
// remaining args on the stack in 8-byte slots.
def CC_Alpha : CallingConv<[
  CCIfType<[i8, i16, i32], CCPromoteToType<i64>>,
  CCIfType<[i64], CCAssignToRegWithShadow<[R16, R17, R18, R19, R20, R21],
                                          [F16, F17, F18, F19, F20, F21]>>,
  CCIfType<[f32, f64], CCAssignToRegWithShadow<[F16, F17, F18, F19, F20, F21],
                                               [R16, R17, R18, R19, R20, R21]>>,
  CCIfType<[i64], CCAssignToStack<8, 8>>,
  CCIfType<[f32, f64], CCAssignToStack<8, 8>>
]>;

// Return values: integers in $0, floating-point in $f0.
def RetCC_Alpha : CallingConv<[
  CCIfType<[i8, i16, i32], CCPromoteToType<i64>>,
  CCIfType<[i64], CCAssignToReg<[R0]>>,
  CCIfType<[f32, f64], CCAssignToReg<[F0]>>
]>;

// Callee-saved integer registers ($9-$15) and floating-point registers
// ($f2-$f9).  $15 is the frame pointer when one is needed.
def CSR_Alpha : CalleeSavedRegs<(add R9, R10, R11, R12, R13, R14, R15,
                                     F2, F3, F4, F5, F6, F7, F8, F9)>;
