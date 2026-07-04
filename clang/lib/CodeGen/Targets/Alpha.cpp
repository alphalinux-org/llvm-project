//===- Alpha.cpp ----------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Alpha ABI Implementation.  The OSF/ELF convention passes the first six
// integer arguments in $16-$21 and the first six floating-point arguments in
// $f16-$f21; scalars wider than a register and small aggregates are handled by
// the generic default ABI.
//===----------------------------------------------------------------------===//

namespace {
class AlphaABIInfo : public DefaultABIInfo {
public:
  AlphaABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}
};

class AlphaTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  AlphaTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<AlphaABIInfo>(CGT)) {}
};
} // end anonymous namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createAlphaTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<AlphaTargetCodeGenInfo>(CGM.getTypes());
}
