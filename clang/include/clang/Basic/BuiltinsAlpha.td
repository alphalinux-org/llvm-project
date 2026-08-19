//===--- BuiltinsAlpha.td - Alpha Builtin function database ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Alpha-specific builtin function database, matching the
// __builtin_alpha_* functions of the GCC Alpha port.  Each is lowered to the
// corresponding llvm.alpha.* intrinsic.
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

class AlphaBuiltin<string prototype> : TargetBuiltin {
  let Spellings = [NAME];
  let Prototype = prototype;
  let Attributes = [NoThrow, Const];
}

class AlphaUnary  : AlphaBuiltin<"long int(long int)">;
class AlphaBinary : AlphaBuiltin<"long int(long int, long int)">;

//===----------------------------------------------------------------------===//
// Base instruction set: always available.
//===----------------------------------------------------------------------===//

def __builtin_alpha_cmpbge : AlphaBinary;
def __builtin_alpha_umulh  : AlphaBinary;
def __builtin_alpha_zap    : AlphaBinary;
def __builtin_alpha_zapnot : AlphaBinary;

def __builtin_alpha_extbl : AlphaBinary;
def __builtin_alpha_extwl : AlphaBinary;
def __builtin_alpha_extll : AlphaBinary;
def __builtin_alpha_extql : AlphaBinary;
def __builtin_alpha_extwh : AlphaBinary;
def __builtin_alpha_extlh : AlphaBinary;
def __builtin_alpha_extqh : AlphaBinary;
def __builtin_alpha_insbl : AlphaBinary;
def __builtin_alpha_inswl : AlphaBinary;
def __builtin_alpha_insll : AlphaBinary;
def __builtin_alpha_insql : AlphaBinary;
def __builtin_alpha_inswh : AlphaBinary;
def __builtin_alpha_inslh : AlphaBinary;
def __builtin_alpha_insqh : AlphaBinary;
def __builtin_alpha_mskbl : AlphaBinary;
def __builtin_alpha_mskwl : AlphaBinary;
def __builtin_alpha_mskll : AlphaBinary;
def __builtin_alpha_mskql : AlphaBinary;
def __builtin_alpha_mskwh : AlphaBinary;
def __builtin_alpha_msklh : AlphaBinary;
def __builtin_alpha_mskqh : AlphaBinary;

def __builtin_alpha_amask   : AlphaUnary;
def __builtin_alpha_implver : AlphaBuiltin<"long int()">;

// rpcc reads a running counter, so it is not const.
def __builtin_alpha_rpcc : TargetBuiltin {
  let Spellings = ["__builtin_alpha_rpcc"];
  let Prototype = "long int()";
  let Attributes = [NoThrow];
}

//===----------------------------------------------------------------------===//
// Count extension (CIX): requires the cix feature.
//===----------------------------------------------------------------------===//

let Features = "cix" in {
  def __builtin_alpha_ctpop : AlphaUnary;
  def __builtin_alpha_ctlz  : AlphaUnary;
  def __builtin_alpha_cttz  : AlphaUnary;
}

//===----------------------------------------------------------------------===//
// Motion video instructions (MVI): requires the mvi feature.
//===----------------------------------------------------------------------===//

let Features = "mvi" in {
  def __builtin_alpha_minub8 : AlphaBinary;
  def __builtin_alpha_minsb8 : AlphaBinary;
  def __builtin_alpha_minuw4 : AlphaBinary;
  def __builtin_alpha_minsw4 : AlphaBinary;
  def __builtin_alpha_maxub8 : AlphaBinary;
  def __builtin_alpha_maxsb8 : AlphaBinary;
  def __builtin_alpha_maxuw4 : AlphaBinary;
  def __builtin_alpha_maxsw4 : AlphaBinary;
  def __builtin_alpha_perr   : AlphaBinary;
  def __builtin_alpha_pklb   : AlphaUnary;
  def __builtin_alpha_pkwb   : AlphaUnary;
  def __builtin_alpha_unpkbl : AlphaUnary;
  def __builtin_alpha_unpkbw : AlphaUnary;
}
