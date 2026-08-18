//===--- Alpha.h - Declare Alpha target feature support ---------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Alpha TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_ALPHA_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_ALPHA_H

#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY AlphaTargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

  // Optional instruction-set extensions, selected by -mcpu or -m<ext> flags.
  bool HasBWX = false;         // Byte/word memory access.
  bool HasCIX = false;         // Count instructions.
  bool HasMVI = false;         // Motion video instructions.
  bool HasFIX = false;         // Float/int register moves and conversions.
  bool HasIEEE = false;        // -mieee: IEEE FP with software completion.
  bool HasIEEEInexact = false; // -mieee-with-inexact.

  std::string CPU = "generic"; // Selected processor (-mcpu).

public:
  AlphaTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    // Little-endian LP64.
    BigEndian = false;
    LongWidth = LongAlign = 64;
    PointerWidth = PointerAlign = 64;
    IntMaxType = SignedLong;
    Int64Type = SignedLong;
    SizeType = UnsignedLong;
    PtrDiffType = SignedLong;
    IntPtrType = SignedLong;
    WCharType = SignedInt;
    WIntType = UnsignedInt;

    // long double is 128-bit IEEE quad (X_floating).
    LongDoubleWidth = LongDoubleAlign = 128;
    LongDoubleFormat = &llvm::APFloat::IEEEquad();

    // ldl_l/stl_c and ldq_l/stq_c give lock-free 32- and 64-bit atomics.
    MaxAtomicPromoteWidth = 128;
    MaxAtomicInlineWidth = 64;

    // 16-byte stack alignment (OSF/ELF ABI).
    SuitableAlign = 128;

    resetDataLayout("e-m:e-p:64:64-i64:64-i128:128-n64-S128");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    // struct { char *__base; int __offset; }, matching the back end's
    // {base, offset} va_list and the GCC/OSF ABI.
    return TargetInfo::AlphaABIBuiltinVaList;
  }

  // Turn the selected processor into the instruction set extensions it
  // implements, matching the ProcessorModel definitions in Alpha.td and the
  // sets GCC's -mcpu= implies.
  bool
  initFeatureMap(llvm::StringMap<bool> &Features, DiagnosticsEngine &Diags,
                 StringRef CPU,
                 const std::vector<std::string> &FeaturesVec) const override {
    bool BWX = false, MVI = false, FIX = false, CIX = false;
    if (CPU == "ev56") {
      BWX = true;
    } else if (CPU == "pca56") {
      BWX = MVI = true;
    } else if (CPU == "ev6") {
      BWX = MVI = FIX = true;
    } else if (CPU == "ev67") {
      BWX = MVI = FIX = CIX = true;
    }
    if (BWX)
      Features["bwx"] = true;
    if (MVI)
      Features["mvi"] = true;
    if (FIX)
      Features["fix"] = true;
    if (CIX)
      Features["cix"] = true;
    return TargetInfo::initFeatureMap(Features, Diags, CPU, FeaturesVec);
  }

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override {
    for (const std::string &Feature : Features) {
      if (Feature == "+bwx")
        HasBWX = true;
      else if (Feature == "+cix")
        HasCIX = true;
      else if (Feature == "+mvi")
        HasMVI = true;
      else if (Feature == "+fix")
        HasFIX = true;
      else if (Feature == "+ieee")
        HasIEEE = true;
      else if (Feature == "+ieee-with-inexact")
        HasIEEE = HasIEEEInexact = true;
    }
    return true;
  }

  bool hasFeature(StringRef Feature) const override {
    return llvm::StringSwitch<bool>(Feature)
        .Case("alpha", true)
        .Case("bwx", HasBWX)
        .Case("cix", HasCIX)
        .Case("mvi", HasMVI)
        .Case("fix", HasFIX)
        .Case("ieee", HasIEEE)
        .Case("ieee-with-inexact", HasIEEEInexact)
        .Default(false);
  }

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    // Constraint letters match GCC's alpha back end
    // (config/alpha/constraints.md).
    switch (*Name) {
    case 'f': // Floating-point register.
    case 'a': // Integer register $24.
    case 'b': // Integer register $25.
    case 'c': // Integer register $27.
    case 'v': // Integer register $0.
      Info.setAllowsRegister();
      return true;
    case 'I': // Unsigned 8-bit constant (logical/arithmetic immediate).
      Info.setRequiresImmediate(0, 255);
      return true;
    case 'J': // The constant zero.
      Info.setRequiresImmediate(0, 0);
      return true;
    case 'K': // Signed 16-bit constant (lda).
      Info.setRequiresImmediate(-32768, 32767);
      return true;
    case 'P': // The constant 1, 2 or 3 (scaled add/shift).
      Info.setRequiresImmediate(1, 3);
      return true;
    case 'S': // Unsigned 6-bit constant (shift count).
      Info.setRequiresImmediate(0, 63);
      return true;
    case 'L': // ldah constant (low 16 bits zero).
    case 'M': // zap byte-mask constant.
    case 'N': // Complemented unsigned 8-bit constant.
    case 'O': // Negated unsigned 8-bit constant.
      Info.setRequiresImmediate();
      return true;
    // GCC also has 'G' for the floating-point zero, 'W' for the vector zero
    // and 'T' for a high-part symbol, all of which it marks @internal.
    // Nothing lowers those here, so they are rejected rather than accepted and
    // handed to a back end with no case for them.
    case 'Q': // A memory operand.
      Info.setAllowsMemory();
      return true;
    case 'R': // A direct-call target: a symbol a bsr can reach.
      return true;
    }
    return false;
  }

  std::string_view getClobbers() const override { return ""; }

  bool isValidCPUName(StringRef Name) const override {
    return Name == "generic" || Name == "ev4" || Name == "ev45" ||
           Name == "ev5" || Name == "ev56" || Name == "pca56" ||
           Name == "ev6" || Name == "ev67";
  }

  bool setCPU(StringRef Name) override {
    CPU = Name.str();
    return isValidCPUName(Name);
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_ALPHA_H
