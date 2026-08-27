//===-- AlphaLegalizerInfo.h ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the targeting of the MachineLegalizer class for Alpha.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_GISEL_ALPHALEGALIZERINFO_H
#define LLVM_LIB_TARGET_ALPHA_GISEL_ALPHALEGALIZERINFO_H

#include "llvm/CodeGen/GlobalISel/LegalizerInfo.h"

namespace llvm {

class AlphaSubtarget;

class AlphaLegalizerInfo : public LegalizerInfo {
public:
  AlphaLegalizerInfo(const AlphaSubtarget &ST);

  bool legalizeCustom(LegalizerHelper &Helper, MachineInstr &MI,
                      LostDebugLocObserver &LocObserver) const override;

  bool legalizeIntrinsic(LegalizerHelper &Helper,
                         MachineInstr &MI) const override;

private:
  bool legalizeVAStart(LegalizerHelper &Helper, MachineInstr &MI) const;
  bool legalizeVACopy(LegalizerHelper &Helper, MachineInstr &MI) const;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_GISEL_ALPHALEGALIZERINFO_H
