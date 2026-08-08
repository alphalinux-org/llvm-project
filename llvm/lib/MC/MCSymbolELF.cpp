//===- lib/MC/MCSymbolELF.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCSymbolELF.h"
#include "llvm/BinaryFormat/ELF.h"

namespace llvm {

namespace {
enum {
  // Shift value for STT_* flags. 7 possible values. 3 bits.
  ELF_STT_Shift = 0,

  // Shift value for STB_* flags. 4 possible values, 2 bits.
  ELF_STB_Shift = 3,

  // Shift value for STV_* flags. 4 possible values, 2 bits.
  ELF_STV_Shift = 5,

  // Shift value for STO_* flags. 5 bits. The processor-specific st_other bits
  // occupy bits 3-7; bits 0-2 are not stored here, so setOther() requires them
  // clear and bit 2 is unrepresentable. Alpha's STO_ALPHA_STD_GPLOAD is 0x88,
  // whose bit 3 is the one a narrower field used to drop, so we shift right by
  // 3 before storing.
  ELF_STO_Shift = 7,

  // One bit.
  ELF_IsSignature_Shift = 12,

  // One bit.
  ELF_Weakref_Shift = 13,

  // One bit.
  ELF_BindingSet_Shift = 14,

  // One bit.
  ELF_IsMemoryTagged_Shift = 15,
};
// Adding another ELF symbol flag, or widening the st_other field, means growing
// MCSymbol::NumFlagsBits first: the shifts above have to fit inside Flags.
static_assert(ELF_IsMemoryTagged_Shift < MCSymbol::NumFlagsBits,
              "ELF symbol flags do not fit in MCSymbol::Flags");
}

void MCSymbolELF::setBinding(unsigned Binding) const {
  setIsBindingSet();
  unsigned Val;
  switch (Binding) {
  default:
    llvm_unreachable("Unsupported Binding");
  case ELF::STB_LOCAL:
    Val = 0;
    break;
  case ELF::STB_GLOBAL:
    Val = 1;
    break;
  case ELF::STB_WEAK:
    Val = 2;
    break;
  case ELF::STB_GNU_UNIQUE:
    Val = 3;
    break;
  }
  uint32_t OtherFlags = getFlags() & ~(0x3 << ELF_STB_Shift);
  setFlags(OtherFlags | (Val << ELF_STB_Shift));
}

unsigned MCSymbolELF::getBinding() const {
  if (isBindingSet()) {
    uint32_t Val = (Flags >> ELF_STB_Shift) & 3;
    switch (Val) {
    default:
      llvm_unreachable("Invalid value");
    case 0:
      return ELF::STB_LOCAL;
    case 1:
      return ELF::STB_GLOBAL;
    case 2:
      return ELF::STB_WEAK;
    case 3:
      return ELF::STB_GNU_UNIQUE;
    }
  }

  if (isDefined())
    return ELF::STB_LOCAL;
  if (isUsedInReloc())
    return ELF::STB_GLOBAL;
  if (isSignature())
    return ELF::STB_LOCAL;
  return ELF::STB_GLOBAL;
}

void MCSymbolELF::setType(unsigned Type) const {
  unsigned Val;
  switch (Type) {
  default:
    llvm_unreachable("Unsupported Binding");
  case ELF::STT_NOTYPE:
    Val = 0;
    break;
  case ELF::STT_OBJECT:
    Val = 1;
    break;
  case ELF::STT_FUNC:
    Val = 2;
    break;
  case ELF::STT_SECTION:
    Val = 3;
    break;
  case ELF::STT_COMMON:
    Val = 4;
    break;
  case ELF::STT_TLS:
    Val = 5;
    break;
  case ELF::STT_GNU_IFUNC:
    Val = 6;
    break;
  }
  uint32_t OtherFlags = getFlags() & ~(0x7 << ELF_STT_Shift);
  setFlags(OtherFlags | (Val << ELF_STT_Shift));
}

unsigned MCSymbolELF::getType() const {
  uint32_t Val = (Flags >> ELF_STT_Shift) & 7;
  switch (Val) {
  default:
    llvm_unreachable("Invalid value");
  case 0:
    return ELF::STT_NOTYPE;
  case 1:
    return ELF::STT_OBJECT;
  case 2:
    return ELF::STT_FUNC;
  case 3:
    return ELF::STT_SECTION;
  case 4:
    return ELF::STT_COMMON;
  case 5:
    return ELF::STT_TLS;
  case 6:
    return ELF::STT_GNU_IFUNC;
  }
}

void MCSymbolELF::setVisibility(unsigned Visibility) {
  assert(Visibility == ELF::STV_DEFAULT || Visibility == ELF::STV_INTERNAL ||
         Visibility == ELF::STV_HIDDEN || Visibility == ELF::STV_PROTECTED);

  uint32_t OtherFlags = getFlags() & ~(0x3 << ELF_STV_Shift);
  setFlags(OtherFlags | (Visibility << ELF_STV_Shift));
}

unsigned MCSymbolELF::getVisibility() const {
  unsigned Visibility = (Flags >> ELF_STV_Shift) & 3;
  return Visibility;
}

void MCSymbolELF::setOther(unsigned Other) {
  assert((Other & 0x7) == 0);
  Other >>= 3;
  assert(Other <= 0x1f);
  uint32_t OtherFlags = getFlags() & ~(0x1f << ELF_STO_Shift);
  setFlags(OtherFlags | (Other << ELF_STO_Shift));
}

unsigned MCSymbolELF::getOther() const {
  unsigned Other = (Flags >> ELF_STO_Shift) & 0x1f;
  return Other << 3;
}

void MCSymbolELF::setIsWeakref() const {
  uint32_t OtherFlags = getFlags() & ~(0x1 << ELF_Weakref_Shift);
  setFlags(OtherFlags | (1 << ELF_Weakref_Shift));
}

bool MCSymbolELF::isWeakref() const {
  return getFlags() & (0x1 << ELF_Weakref_Shift);
}

void MCSymbolELF::setIsSignature() const {
  uint32_t OtherFlags = getFlags() & ~(0x1 << ELF_IsSignature_Shift);
  setFlags(OtherFlags | (1 << ELF_IsSignature_Shift));
}

bool MCSymbolELF::isSignature() const {
  return getFlags() & (0x1 << ELF_IsSignature_Shift);
}

void MCSymbolELF::setIsBindingSet() const {
  uint32_t OtherFlags = getFlags() & ~(0x1 << ELF_BindingSet_Shift);
  setFlags(OtherFlags | (1 << ELF_BindingSet_Shift));
}

bool MCSymbolELF::isBindingSet() const {
  return getFlags() & (0x1 << ELF_BindingSet_Shift);
}

bool MCSymbolELF::isMemtag() const {
  return getFlags() & (0x1 << ELF_IsMemoryTagged_Shift);
}

void MCSymbolELF::setMemtag(bool Tagged) {
  uint32_t OtherFlags = getFlags() & ~(1 << ELF_IsMemoryTagged_Shift);
  if (Tagged)
    setFlags(OtherFlags | (1 << ELF_IsMemoryTagged_Shift));
  else
    setFlags(OtherFlags);
}
}
