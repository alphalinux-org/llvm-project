//===-- AlphaMCAsmInfo.h - Alpha Asm Info -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the AlphaMCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCASMINFO_H
#define LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;
class MCTargetOptions;

class AlphaMCAsmInfo : public MCAsmInfoELF {
public:
  explicit AlphaMCAsmInfo(const Triple &TT, const MCTargetOptions &Options);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_MCTARGETDESC_ALPHAMCASMINFO_H
