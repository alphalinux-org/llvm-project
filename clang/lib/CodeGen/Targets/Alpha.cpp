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

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override;
};

class AlphaTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  AlphaTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<AlphaABIInfo>(CGT)) {}
};
} // end anonymous namespace

RValue AlphaABIInfo::EmitVAArg(CodeGenFunction &CGF, Address VAListAddr,
                               QualType Ty, AggValueSlot Slot) const {
  // va_list is  struct { char *__base; int __offset; }.  Argument slot N sits
  // at __base + N*8 for every N: slots 0-5 are the integer argument register
  // save area and slots 6 onwards are the caller's stack arguments, which the
  // prologue arranged to follow it.  A scalar floating-point argument that
  // landed in one of the six argument registers is read from the
  // floating-point save area instead, which sits 48 bytes below __base.
  //
  // This cannot use the generic va_arg instruction: an aggregate is passed by
  // value as several consecutive slots, which va_arg cannot describe.
  CGBuilderTy &Builder = CGF.Builder;
  CharUnits SlotSize = CharUnits::fromQuantity(8);

  ABIArgInfo AI = classifyArgumentType(Ty);
  llvm::Type *ArgTy = CGF.ConvertTypeForMem(Ty);

  if (AI.isIgnore())
    return Slot.asRValue();

  bool IsIndirect = AI.isIndirect();

  // How many 8-byte slots the argument occupies.  An indirect argument is
  // passed as a pointer, so it always takes exactly one.
  CharUnits ArgSize =
      IsIndirect ? SlotSize
                 : getContext().getTypeSizeInChars(Ty).alignTo(SlotSize);

  Address BaseAddr = Builder.CreateStructGEP(VAListAddr, 0, "ap.base.addr");
  Address OffsetAddr = Builder.CreateStructGEP(VAListAddr, 1, "ap.offset.addr");
  llvm::Value *Base = Builder.CreateLoad(BaseAddr, "ap.base");
  llvm::Value *Offset = Builder.CreateLoad(OffsetAddr, "ap.offset");

  // Bias the address (but not the stored offset) into the floating-point save
  // area for a floating-point argument still within the six register slots.
  llvm::Value *EffOffset = Offset;
  if (!IsIndirect && Ty->isRealFloatingType()) {
    llvm::Value *InRegs = Builder.CreateICmpULT(
        Offset, llvm::ConstantInt::get(CGF.Int32Ty, 48), "ap.in.regs");
    EffOffset = Builder.CreateSelect(
        InRegs,
        Builder.CreateSub(Offset, llvm::ConstantInt::get(CGF.Int32Ty, 48)),
        Offset, "ap.eff.offset");
  }

  // __offset is an int, so widen it before using it as an address delta.
  llvm::Value *Cur = Builder.CreateGEP(
      CGF.Int8Ty, Base, Builder.CreateSExt(EffOffset, CGF.Int64Ty), "ap.cur");

  // Advance past the slots this argument consumed.
  llvm::Value *Next = Builder.CreateAdd(
      Offset, llvm::ConstantInt::get(CGF.Int32Ty, ArgSize.getQuantity()),
      "ap.next");
  Builder.CreateStore(Next, OffsetAddr);

  // The save area and the stack argument list are only slot-aligned, so an
  // argument read out of them is too, regardless of the type's own alignment.
  llvm::Type *SlotTy =
      IsIndirect ? llvm::PointerType::getUnqual(CGF.getLLVMContext()) : ArgTy;
  Address ArgAddr(Cur, SlotTy, SlotSize);
  if (IsIndirect)
    ArgAddr = Address(Builder.CreateLoad(ArgAddr, "ap.indirect"), ArgTy,
                      getContext().getTypeAlignInChars(Ty));

  return CGF.EmitLoadOfAnyValue(CGF.MakeAddrLValue(ArgAddr, Ty), Slot);
}

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createAlphaTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<AlphaTargetCodeGenInfo>(CGM.getTypes());
}
