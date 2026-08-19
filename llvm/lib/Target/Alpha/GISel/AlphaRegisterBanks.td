//===-- AlphaRegisterBanks.td - Describe the Alpha banks ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Define the Alpha register banks used for GlobalISel.  A value lives either in
// an integer register or in a floating-point one; there is no other choice, and
// moving between the two costs a memory round trip (or itofs/ftois on ev6).
//
//===----------------------------------------------------------------------===//

def GPRRegBank : RegisterBank<"GPR", [GPRC]>;
def FPRRegBank : RegisterBank<"FPR", [FPRC]>;
