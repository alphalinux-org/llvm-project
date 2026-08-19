//===-- AlphaRegisterBankInfo.h ---------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the targeting of the RegisterBankInfo class for Alpha.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAREGISTERBANKINFO_H
#define LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAREGISTERBANKINFO_H

#include "llvm/CodeGen/RegisterBankInfo.h"

#define GET_REGBANK_DECLARATIONS
#include "AlphaGenRegisterBank.inc"
#undef GET_REGBANK_DECLARATIONS

namespace llvm {

class TargetRegisterInfo;

class AlphaGenRegisterBankInfo : public RegisterBankInfo {
protected:
#define GET_TARGET_REGBANK_CLASS
#include "AlphaGenRegisterBank.inc"
#undef GET_TARGET_REGBANK_CLASS
};

class AlphaRegisterBankInfo final : public AlphaGenRegisterBankInfo {
public:
  AlphaRegisterBankInfo();

  const InstructionMapping &
  getInstrMapping(const MachineInstr &MI) const override;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_ALPHA_GISEL_ALPHAREGISTERBANKINFO_H
