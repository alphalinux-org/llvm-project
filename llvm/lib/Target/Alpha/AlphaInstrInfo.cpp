//===-- AlphaInstrInfo.cpp - Alpha Instruction Information ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaInstrInfo.h"
#include "AlphaSubtarget.h"
#include "MCTargetDesc/AlphaMCTargetDesc.h"

#define GET_INSTRINFO_CTOR_DTOR
#include "AlphaGenInstrInfo.inc"

using namespace llvm;

AlphaInstrInfo::AlphaInstrInfo(const AlphaSubtarget &STI)
    : AlphaGenInstrInfo(STI, RI, Alpha::ADJCALLSTACKDOWN,
                        Alpha::ADJCALLSTACKUP),
      RI() {}
