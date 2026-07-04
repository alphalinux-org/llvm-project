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
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY AlphaTargetInfo : public TargetInfo {
  static const char *const GCCRegNames[];

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

    // 16-byte stack alignment (OSF/ELF ABI).
    SuitableAlign = 128;

    resetDataLayout("e-m:e-p:64:64-i64:64-i128:128-n64-S128");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  llvm::SmallVector<Builtin::InfosShard> getTargetBuiltins() const override {
    return {};
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "alpha";
  }

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return {};
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    switch (*Name) {
    case 'f': // Floating-point register.
      Info.setAllowsRegister();
      return true;
    }
    return false;
  }

  std::string_view getClobbers() const override { return ""; }

  bool isValidCPUName(StringRef Name) const override {
    return Name == "generic" || Name == "ev4" || Name == "ev5" ||
           Name == "ev56" || Name == "ev6" || Name == "ev67";
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_ALPHA_H
