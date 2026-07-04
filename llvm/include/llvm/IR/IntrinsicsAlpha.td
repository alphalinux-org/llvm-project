//===- IntrinsicsAlpha.td - Defines Alpha intrinsics -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Alpha-specific intrinsics, exposed to C as the
// __builtin_alpha_* functions that the GCC Alpha port provides.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "alpha" in {

// A pure operation on one or two 64-bit registers.
class AlphaI64_1
    : DefaultAttrsIntrinsic<[llvm_i64_ty], [llvm_i64_ty], [IntrNoMem]>;
class AlphaI64_2
    : DefaultAttrsIntrinsic<[llvm_i64_ty], [llvm_i64_ty, llvm_i64_ty],
                            [IntrNoMem]>;

//===----------------------------------------------------------------------===//
// Base instruction set.
//===----------------------------------------------------------------------===//

def int_alpha_cmpbge : ClangBuiltin<"__builtin_alpha_cmpbge">, AlphaI64_2;
def int_alpha_umulh  : ClangBuiltin<"__builtin_alpha_umulh">,  AlphaI64_2;
def int_alpha_zap    : ClangBuiltin<"__builtin_alpha_zap">,    AlphaI64_2;
def int_alpha_zapnot : ClangBuiltin<"__builtin_alpha_zapnot">, AlphaI64_2;

// Byte/word/longword extract, insert and mask, low and high.
def int_alpha_extbl : ClangBuiltin<"__builtin_alpha_extbl">, AlphaI64_2;
def int_alpha_extwl : ClangBuiltin<"__builtin_alpha_extwl">, AlphaI64_2;
def int_alpha_extll : ClangBuiltin<"__builtin_alpha_extll">, AlphaI64_2;
def int_alpha_extql : ClangBuiltin<"__builtin_alpha_extql">, AlphaI64_2;
def int_alpha_extwh : ClangBuiltin<"__builtin_alpha_extwh">, AlphaI64_2;
def int_alpha_extlh : ClangBuiltin<"__builtin_alpha_extlh">, AlphaI64_2;
def int_alpha_extqh : ClangBuiltin<"__builtin_alpha_extqh">, AlphaI64_2;
def int_alpha_insbl : ClangBuiltin<"__builtin_alpha_insbl">, AlphaI64_2;
def int_alpha_inswl : ClangBuiltin<"__builtin_alpha_inswl">, AlphaI64_2;
def int_alpha_insll : ClangBuiltin<"__builtin_alpha_insll">, AlphaI64_2;
def int_alpha_insql : ClangBuiltin<"__builtin_alpha_insql">, AlphaI64_2;
def int_alpha_inswh : ClangBuiltin<"__builtin_alpha_inswh">, AlphaI64_2;
def int_alpha_inslh : ClangBuiltin<"__builtin_alpha_inslh">, AlphaI64_2;
def int_alpha_insqh : ClangBuiltin<"__builtin_alpha_insqh">, AlphaI64_2;
def int_alpha_mskbl : ClangBuiltin<"__builtin_alpha_mskbl">, AlphaI64_2;
def int_alpha_mskwl : ClangBuiltin<"__builtin_alpha_mskwl">, AlphaI64_2;
def int_alpha_mskll : ClangBuiltin<"__builtin_alpha_mskll">, AlphaI64_2;
def int_alpha_mskql : ClangBuiltin<"__builtin_alpha_mskql">, AlphaI64_2;
def int_alpha_mskwh : ClangBuiltin<"__builtin_alpha_mskwh">, AlphaI64_2;
def int_alpha_msklh : ClangBuiltin<"__builtin_alpha_msklh">, AlphaI64_2;
def int_alpha_mskqh : ClangBuiltin<"__builtin_alpha_mskqh">, AlphaI64_2;

// Query the architecture feature mask.
def int_alpha_amask : ClangBuiltin<"__builtin_alpha_amask">, AlphaI64_1;

// Report the processor implementation version.
def int_alpha_implver : ClangBuiltin<"__builtin_alpha_implver">,
    DefaultAttrsIntrinsic<[llvm_i64_ty], [], [IntrNoMem]>;

// Read the process cycle counter.  It touches no memory, but its value changes
// over time, so it has a side effect and must not be merged or eliminated.
def int_alpha_rpcc : ClangBuiltin<"__builtin_alpha_rpcc">,
    DefaultAttrsIntrinsic<[llvm_i64_ty], [], [IntrNoMem, IntrHasSideEffects]>;

//===----------------------------------------------------------------------===//
// Count extension (CIX).
//===----------------------------------------------------------------------===//

def int_alpha_ctpop : ClangBuiltin<"__builtin_alpha_ctpop">, AlphaI64_1;
def int_alpha_ctlz  : ClangBuiltin<"__builtin_alpha_ctlz">,  AlphaI64_1;
def int_alpha_cttz  : ClangBuiltin<"__builtin_alpha_cttz">,  AlphaI64_1;

//===----------------------------------------------------------------------===//
// Motion video instructions (MVI, enabled by the mvi feature).
//===----------------------------------------------------------------------===//

def int_alpha_minub8 : ClangBuiltin<"__builtin_alpha_minub8">, AlphaI64_2;
def int_alpha_minsb8 : ClangBuiltin<"__builtin_alpha_minsb8">, AlphaI64_2;
def int_alpha_minuw4 : ClangBuiltin<"__builtin_alpha_minuw4">, AlphaI64_2;
def int_alpha_minsw4 : ClangBuiltin<"__builtin_alpha_minsw4">, AlphaI64_2;
def int_alpha_maxub8 : ClangBuiltin<"__builtin_alpha_maxub8">, AlphaI64_2;
def int_alpha_maxsb8 : ClangBuiltin<"__builtin_alpha_maxsb8">, AlphaI64_2;
def int_alpha_maxuw4 : ClangBuiltin<"__builtin_alpha_maxuw4">, AlphaI64_2;
def int_alpha_maxsw4 : ClangBuiltin<"__builtin_alpha_maxsw4">, AlphaI64_2;
def int_alpha_perr   : ClangBuiltin<"__builtin_alpha_perr">,   AlphaI64_2;
def int_alpha_pklb   : ClangBuiltin<"__builtin_alpha_pklb">,   AlphaI64_1;
def int_alpha_pkwb   : ClangBuiltin<"__builtin_alpha_pkwb">,   AlphaI64_1;
def int_alpha_unpkbl : ClangBuiltin<"__builtin_alpha_unpkbl">, AlphaI64_1;
def int_alpha_unpkbw : ClangBuiltin<"__builtin_alpha_unpkbw">, AlphaI64_1;

} // TargetPrefix = "alpha"
