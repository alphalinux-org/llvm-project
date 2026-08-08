//===--- Alpha.cpp - Implement Alpha target feature support ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Alpha TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Alpha.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    clang::Alpha::LastTSBuiltin - Builtin::FirstTSBuiltin;

#define GET_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsAlpha.inc"
#undef GET_BUILTIN_STR_TABLE

static constexpr Builtin::Info BuiltinInfos[] = {
#define GET_BUILTIN_INFOS
#include "clang/Basic/BuiltinsAlpha.inc"
#undef GET_BUILTIN_INFOS
};
static_assert(std::size(BuiltinInfos) == NumBuiltins);

llvm::SmallVector<Builtin::InfosShard>
AlphaTargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

const char *const AlphaTargetInfo::GCCRegNames[] = {
    "$0",   "$1",   "$2",   "$3",   "$4",   "$5",   "$6",   "$7",
    "$8",   "$9",   "$10",  "$11",  "$12",  "$13",  "$14",  "$15",
    "$16",  "$17",  "$18",  "$19",  "$20",  "$21",  "$22",  "$23",
    "$24",  "$25",  "$26",  "$27",  "$28",  "$29",  "$30",  "$31",
    "$f0",  "$f1",  "$f2",  "$f3",  "$f4",  "$f5",  "$f6",  "$f7",
    "$f8",  "$f9",  "$f10", "$f11", "$f12", "$f13", "$f14", "$f15",
    "$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23",
    "$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31"};

ArrayRef<const char *> AlphaTargetInfo::getGCCRegNames() const {
  return llvm::ArrayRef(GCCRegNames);
}

void AlphaTargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__alpha__");
  Builder.defineMacro("__alpha");
  Builder.defineMacro("_LP64");
  Builder.defineMacro("__LP64__");

  // Processor-family macro, matching GCC: the EV56/PCA56 share the EV5 core and
  // EV67 shares the EV6 core, so the family macro follows the microarchitecture
  // rather than the exact model.
  StringRef Family = "__alpha_ev4__";
  if (CPU == "ev5" || CPU == "ev56" || CPU == "pca56")
    Family = "__alpha_ev5__";
  else if (CPU == "ev6" || CPU == "ev67")
    Family = "__alpha_ev6__";
  Builder.defineMacro(Family);

  // Instruction-set feature macros, matching GCC's -m<ext> defines.
  if (HasBWX)
    Builder.defineMacro("__alpha_bwx__");
  if (HasMVI)
    Builder.defineMacro("__alpha_max__");
  if (HasFIX)
    Builder.defineMacro("__alpha_fix__");
  if (HasCIX)
    Builder.defineMacro("__alpha_cix__");
  // Match GCC (config/alpha/alpha.h): -mieee defines _IEEE_FP, and
  // -mieee-with-inexact also _IEEE_FP_INEXACT.
  if (HasIEEE)
    Builder.defineMacro("_IEEE_FP");
  if (HasIEEEInexact)
    Builder.defineMacro("_IEEE_FP_INEXACT");
}
