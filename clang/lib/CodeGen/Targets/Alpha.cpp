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
// arguments in $16-$21 (integer) or $f16-$f21 (floating point), sharing one
// slot index.  Aggregates are passed by value as a sequence of 8-byte pieces in
// the integer registers (spilling to the stack once the registers run out) and
// are returned in memory through a hidden pointer.
//===----------------------------------------------------------------------===//

namespace {
class AlphaABIInfo : public DefaultABIInfo {
public:
  AlphaABIInfo(CodeGenTypes &CGT) : DefaultABIInfo(CGT) {}

  ABIArgInfo classifyArgumentType(QualType Ty, bool IsNamed) const;
  ABIArgInfo classifyReturnType(QualType Ty) const;
  ABIArgInfo extendIntegerInRegister(QualType Ty) const;

  void computeInfo(CGFunctionInfo &FI) const override {
    if (!getCXXABI().classifyReturnType(FI))
      FI.getReturnInfo() = classifyReturnType(FI.getReturnType());
    unsigned ArgNo = 0, NumRequired = FI.getNumRequiredArgs();
    for (auto &I : FI.arguments())
      I.info = classifyArgumentType(I.type, ArgNo++ < NumRequired);
  }

  RValue EmitVAArg(CodeGenFunction &CGF, Address VAListAddr, QualType Ty,
                   AggValueSlot Slot) const override;
};

// long double is the 128-bit X_floating format, which the ABI passes and
// returns in memory (by an invisible reference / a hidden result pointer)
// rather than in registers.  _Complex long double (TCmode) goes the same way,
// matching GCC's alpha_pass_by_reference and alpha_return_in_memory.
static bool isXFloating(const ASTContext &Ctx, QualType Ty) {
  if (const auto *CT = Ty->getAs<ComplexType>())
    return CT->getElementType()->isRealFloatingType() &&
           Ctx.getTypeSize(CT->getElementType()) == 128;
  return Ty->isRealFloatingType() && Ctx.getTypeSize(Ty) == 128;
}

// The S_floating types: float and _Complex float (SFmode and SCmode).
static bool isSFloating(const ASTContext &Ctx, QualType Ty) {
  if (const auto *CT = Ty->getAs<ComplexType>())
    return CT->getElementType()->isRealFloatingType() &&
           Ctx.getTypeSize(CT->getElementType()) == 32;
  return Ty->isRealFloatingType() && Ctx.getTypeSize(Ty) == 32;
}

// gcc gives a struct whose layout is one member the *mode* of that member --
// compute_record_mode does it, and it recurses, so `struct { struct { float f;
// } i; }` and `struct { float f[1]; }` reach SFmode too -- and then
// alpha_function_arg and alpha_pass_by_reference classify the struct by that
// mode rather than as an aggregate.  For most modes that changes nothing here,
// because an aggregate of that size is passed in the same integer slots
// anyway; it matters only for the two modes that go by invisible reference,
// TFmode/TCmode and (unnamed) SFmode/SCmode.  Returns the scalar type the
// struct inherits from, or a null QualType.
//
// A union never inherits: `union { float f; }` has one member and is still
// passed by value, which is why this asks for a struct and not merely for a
// record with one field.  Neither does a struct whose member does not fill it
// (`struct { long double x; int pad; }`) or whose array member has more than
// one element.
//
// compute_record_mode (gcc/stor-layout.cc) walks every FIELD_DECL and takes
// the mode of the one that fills the record, so three shapes that look like
// more than one member reach a mode here too:
//
//  - a member whose DECL_SIZE is zero is walked over, not counted: a
//    zero-length array, a GNU empty struct, a zero-width bitfield.  (A C++
//    empty *member* has size 1 and does count.)
//  - a C++ base is a FIELD_DECL like any other, so an empty base is walked
//    over and a single non-empty one is the member that fills the record.
//  - under STRICT_ALIGNMENT, which Alpha is, a record whose alignment is below
//    the mode's keeps BLKmode -- so a `packed' struct around a float or a long
//    double is an aggregate, not SFmode or TFmode.
static QualType getInheritedScalarType(const ASTContext &Ctx, QualType Ty) {
  QualType OrigTy = Ty;
  uint64_t Size = Ctx.getTypeSize(Ty);
  while (true) {
    if (const auto *AT = Ctx.getAsConstantArrayType(Ty)) {
      if (!AT->getSize().isOne())
        return QualType();
      Ty = AT->getElementType();
      continue;
    }
    const auto *RT = Ty->getAs<RecordType>();
    if (!RT)
      break;
    const RecordDecl *RD = RT->getDecl();
    if (!RD->isStruct() && !RD->isClass())
      return QualType();
    QualType Field;
    // A member that occupies nothing is skipped rather than counted, so it
    // cannot be the second member that makes the record an aggregate.
    auto AddMember = [&](QualType MemberTy) {
      if (Ctx.getTypeSize(MemberTy) == 0)
        return true;
      if (!Field.isNull())
        return false;
      Field = MemberTy;
      return true;
    };
    if (const auto *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
      // A virtual base has no fixed offset and a polymorphic class carries a
      // vptr the walk cannot see; neither can hand its mode on.
      if (CXXRD->getNumVBases() != 0 || CXXRD->isPolymorphic())
        return QualType();
      for (const CXXBaseSpecifier &B : CXXRD->bases()) {
        // An empty base occupies no storage at all -- the empty base
        // optimization gives it offset and size zero, where the same class as
        // a *member* would still be one byte -- so it is walked over.
        if (B.getType()->getAsCXXRecordDecl()->isEmpty())
          continue;
        if (!AddMember(B.getType()))
          return QualType();
      }
    }
    for (const FieldDecl *FD : RD->fields()) {
      // A zero-width bitfield names no storage and is walked over; any other
      // bitfield is a member the record cannot take the mode of.
      if (FD->isBitField()) {
        if (FD->getBitWidthValue() != 0)
          return QualType();
        continue;
      }
      if (!AddMember(FD->getType()))
        return QualType();
    }
    if (Field.isNull())
      return QualType();
    Ty = Field;
  }
  // The member must fill the struct, or the struct keeps BLKmode.
  if (Ctx.getTypeSize(Ty) != Size)
    return QualType();
  if (!Ty->isRealFloatingType() && !Ty->getAs<ComplexType>())
    return QualType();
  // Alpha is STRICT_ALIGNMENT, so a record aligned below its would-be mode
  // stays BLKmode.  `struct { long double x; } __attribute__((packed))' is
  // aligned to 1 and is passed as an aggregate, not by reference.
  if (Ctx.getTypeAlignInChars(OrigTy) < Ctx.getTypeAlignInChars(Ty))
    return QualType();
  return Ty;
}

class AlphaTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  AlphaTargetCodeGenInfo(CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<AlphaABIInfo>(CGT)) {}

  // An unnamed _Complex float is passed as two pointers in two argument slots,
  // one per half, because gcc's alpha_split_complex_arg splits SCmode into two
  // SFmode arguments before alpha_pass_by_reference classifies either one, and
  // an unnamed SFmode argument goes by reference.  Splitting the call argument
  // is the only way to say that: an indirect ABIArgInfo yields one pointer, and
  // there are two.  The other complex modes need no split here -- their halves
  // are passed the same way in the same two slots whether they are classified
  // as a pair or one at a time -- and _Complex long double is already indirect
  // as a whole, matching gcc's TCmode exception.
  bool shouldSplitComplexVariadicArg(QualType Ty) const override {
    return Ty->getAs<ComplexType>() &&
           isSFloating(getABIInfo().getContext(), Ty);
  }
};
} // end anonymous namespace

// Extension rules for an integer narrower than a register, matching GCC's
// alpha_promote_function_mode.  A 32-bit value is always sign-extended, even
// when its type is unsigned: the hardware's 32-bit arithmetic produces a
// sign-extended result, so that is the canonical register form.  Anything
// narrower has no such instructions behind it and is extended according to the
// signedness of its type, so _Bool, unsigned char and unsigned short are
// zero-extended.  Getting _Bool wrong is particularly damaging, since a `true`
// sign-extended to all-ones is not the 0/1 the rest of the world expects.
ABIArgInfo AlphaABIInfo::extendIntegerInRegister(QualType Ty) const {
  if (getContext().getTypeSize(Ty) == 32)
    return ABIArgInfo::getSignExtend(Ty);
  return ABIArgInfo::getExtend(Ty);
}

ABIArgInfo AlphaABIInfo::classifyReturnType(QualType Ty) const {
  if (isXFloating(getContext(), Ty))
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  // A complex float or complex double comes back in $f0/$f1: GCC judges a
  // complex float type by the width of one part, not of the pair
  // (alpha_return_in_memory), so only complex long double goes to memory, and
  // isXFloating above has already caught that.
  if (const auto *CT = Ty->getAs<ComplexType>()) {
    if (CT->getElementType()->isRealFloatingType())
      return ABIArgInfo::getDirect();
    // A complex integer is judged the same way, by the width of one part, so
    // one that fits a register comes back packed in $0 rather than in memory.
    if (getContext().getTypeSize(Ty) <= 64)
      return ABIArgInfo::getDirect(llvm::Type::getInt64Ty(getVMContext()));
  }
  // A vector of floating-point elements comes back in memory whatever its
  // width -- gcc's alpha_return_in_memory takes MODE_VECTOR_FLOAT the way it
  // takes an aggregate, with the comment "Pass all float vectors in memory,
  // like an aggregate" -- and an integer vector joins it once it is wider than
  // a register, by the same size > UNITS_PER_WORD rule everything else uses.
  // One that fits comes back packed in $0.
  if (const auto *VT = Ty->getAs<VectorType>()) {
    if (VT->getElementType()->isRealFloatingType() ||
        getContext().getTypeSize(Ty) > 64)
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     /*ByVal=*/false);
    return ABIArgInfo::getDirect(llvm::Type::getInt64Ty(getVMContext()));
  }

  // An aggregate comes back in memory through a hidden pointer the caller
  // passes in $16; only $0 and $f0 carry a returned value.
  if (isAggregateTypeForABI(Ty))
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  // Anything else too wide for a register is returned in memory as well:
  // __int128, _Complex long, and so on.
  if (getContext().getTypeSize(Ty) > 64)
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/false);
  if (Ty->isIntegralOrEnumerationType() && getContext().getTypeSize(Ty) < 64)
    return extendIntegerInRegister(Ty);
  return DefaultABIInfo::classifyReturnType(Ty);
}

ABIArgInfo AlphaABIInfo::classifyArgumentType(QualType Ty, bool IsNamed) const {
  Ty = useFirstFieldIfTransparentUnion(Ty);

  // long double is passed by an invisible reference to a caller-made copy,
  // matching gcc's alpha_pass_by_reference for TFmode and TCmode.  byval is how
  // that is spelled here: the Alpha calling convention gives such an argument
  // one register holding the address of the copy, not the 16 bytes themselves,
  // so the arguments after it are unaffected.  It also stops the call being
  // tail-called, which matters -- the epilogue runs before the jump, leaving
  // the copy below the stack pointer by the time the callee reads it.
  // A struct that inherits a floating mode from its single member is passed
  // the way that mode is passed, so the two by-reference tests below are asked
  // about the inherited type and not only about Ty itself.  Null for anything
  // that is not such a struct, in which case ModeTy stays Ty and the two tests
  // ask exactly what they asked before.
  QualType ModeTy = Ty;
  bool InheritedMode = false;
  if (isAggregateTypeForABI(Ty)) {
    QualType Inherited = getInheritedScalarType(getContext(), Ty);
    if (!Inherited.isNull()) {
      ModeTy = Inherited;
      InheritedMode = true;
    }
  }

  // Pass a non-trivial C++ record the way its special members require, ahead
  // of the by-reference tests below: those spell "by reference" as byval, and
  // byval is a copy the call lowering makes bitwise.  A record that is
  // RAA_Indirect is already by reference in the ABI sense -- the caller passes
  // the address of the object it constructed, which is also what gcc does,
  // aggregate_type_p sending it to the integer registers as an address -- so
  // copying it here would destroy the wrong storage.  Only a trivially
  // copyable record may take the byval path.
  if (isAggregateTypeForABI(Ty))
    if (CGCXXABI::RecordArgABI RAA = getRecordArgABI(Ty, getCXXABI()))
      return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                     RAA == CGCXXABI::RAA_DirectInMemory);

  if (isXFloating(getContext(), ModeTy))
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/true);

  // An unnamed float goes by reference, which is what gcc's
  // alpha_pass_by_reference does for SFmode.  A 32-bit load into a
  // floating-point register rewrites the bits into the canonical 64-bit form,
  // so the four bytes such an argument would occupy in the register save area
  // are not the value the callee needs to read back.  Default argument
  // promotion keeps plain float out of this; _Float32 reaches it.
  //
  // An unnamed _Complex float is passed as two such references, one per half,
  // because gcc splits SCmode into two SFmode arguments in
  // alpha_split_complex_arg before alpha_pass_by_reference sees either one.
  // The split happens at the call site, where both halves are in hand
  // (shouldSplitComplexVariadicArg), so what reaches this function is two
  // independent floats and the case below is only about a real one.  va_arg
  // reads the pair back the same way; see EmitVAArg.
  //
  // A struct that inherits SFmode or SCmode from its single member goes by
  // reference too, and unlike a bare _Complex float it is *not* split: the
  // split is alpha_split_complex_arg's, which acts on a complex argument and
  // not on a record that merely has complex mode, so the struct is one address
  // in one slot where the bare type is two.  shouldSplitComplexVariadicArg
  // asks about Ty and so already declines to split it.
  if (!IsNamed && isSFloating(getContext(), ModeTy) &&
      (InheritedMode || !Ty->getAs<ComplexType>()))
    return getNaturalAlignIndirect(Ty, getDataLayout().getAllocaAddrSpace(),
                                   /*ByVal=*/true);

  // A complex value is passed as its two parts, in two consecutive argument
  // registers, not packed into one the way an aggregate is.  gcc's
  // alpha_split_complex_arg splits every complex type except TCmode, which
  // isXFloating has already sent to memory above, so a complex integer is
  // split here too.
  if (Ty->getAs<ComplexType>())
    return ABIArgInfo::getDirect();

  // A vector is passed in the integer registers, packed, taking as many
  // quadwords as it occupies.  gcc's alpha_function_arg sends an argument to
  // the floating-point registers only when its mode class is MODE_FLOAT; a
  // vector's is MODE_VECTOR_INT or MODE_VECTOR_FLOAT, so it takes basereg 16
  // like everything else and ALPHA_ARG_SIZE quadwords of the argument list.
  // Without this a vector is neither an aggregate nor a complex nor an
  // integer here, so it reaches a back end with no legal vector type and is
  // scalarised one register per element -- shifting every argument after it.
  if (Ty->isVectorType()) {
    uint64_t Size = getContext().getTypeSize(Ty);
    uint64_t NumRegs = (Size + 63) / 64;
    llvm::Type *I64 = llvm::Type::getInt64Ty(getVMContext());
    return ABIArgInfo::getDirect(
        NumRegs == 1 ? I64 : llvm::ArrayType::get(I64, NumRegs));
  }

  if (isAggregateTypeForABI(Ty)) {
    uint64_t Size = getContext().getTypeSize(Ty);
    if (Size == 0)
      return ABIArgInfo::getIgnore();

    // Pass the aggregate by value as N consecutive 64-bit quadwords; the
    // calling convention places the first six in registers and the rest on the
    // stack.
    uint64_t NumRegs = (Size + 63) / 64;
    llvm::Type *I64 = llvm::Type::getInt64Ty(getVMContext());
    llvm::Type *Coerced =
        NumRegs == 1 ? I64 : llvm::ArrayType::get(I64, NumRegs);
    return ABIArgInfo::getDirect(Coerced);
  }

  if (Ty->isIntegralOrEnumerationType() && getContext().getTypeSize(Ty) < 64)
    return extendIntegerInRegister(Ty);

  return DefaultABIInfo::classifyArgumentType(Ty);
}

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

  // va_arg always fetches an unnamed argument.
  ABIArgInfo AI = classifyArgumentType(Ty, /*IsNamed=*/false);
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

  // Take one slot as SlotQTy and advance past it.  A slot is always 8 bytes,
  // even for a float.
  auto emitSlot = [&](QualType SlotQTy) -> Address {
    llvm::Value *Off = Builder.CreateLoad(OffsetAddr, "ap.offset");
    llvm::Value *Eff = Off;
    if (SlotQTy->isRealFloatingType()) {
      llvm::Value *InRegs = Builder.CreateICmpULT(
          Off, llvm::ConstantInt::get(CGF.Int32Ty, 48), "ap.in.regs");
      Eff = Builder.CreateSelect(
          InRegs,
          Builder.CreateSub(Off, llvm::ConstantInt::get(CGF.Int32Ty, 48)), Off,
          "ap.eff.offset");
    }
    llvm::Value *Cur = Builder.CreateGEP(
        CGF.Int8Ty, Base, Builder.CreateSExt(Eff, CGF.Int64Ty), "ap.cur");
    Builder.CreateStore(
        Builder.CreateAdd(Off, llvm::ConstantInt::get(CGF.Int32Ty, 8),
                          "ap.next"),
        OffsetAddr);
    return Address(Cur, CGF.ConvertTypeForMem(SlotQTy), SlotSize);
  };

  // An unnamed _Complex float was split at the call site into two arguments,
  // each a pointer to a caller-made copy of one half (see
  // shouldSplitComplexVariadicArg).  So it is two slots, each holding a
  // pointer, and each pointer is dereferenced for one float.  The slots hold
  // addresses rather than floating-point values, so they come from the integer
  // save area and take no bias.  gcc reads the same two slots the same way:
  // alpha_gimplify_va_arg_1 recurses on the element type, and each recursion
  // finds an indirect SFmode argument.
  if (const auto *CT = Ty->getAs<ComplexType>())
    if (isSFloating(getContext(), Ty)) {
      QualType EltTy = CT->getElementType();
      QualType PtrTy = getContext().getPointerType(EltTy);
      CharUnits EltAlign = getContext().getTypeAlignInChars(EltTy);
      llvm::Type *EltIRTy = CGF.ConvertTypeForMem(EltTy);
      auto emitHalf = [&](const char *Name) -> llvm::Value * {
        llvm::Value *P = Builder.CreateLoad(emitSlot(PtrTy), "ap.indirect");
        return Builder.CreateLoad(Address(P, EltIRTy, EltAlign), Name);
      };
      llvm::Value *Real = emitHalf("ap.real");
      llvm::Value *Imag = emitHalf("ap.imag");
      return RValue::getComplex(Real, Imag);
    }

  // Any other _Complex is passed as its two parts in two consecutive argument
  // registers, so each part is fetched the way a scalar of the element type
  // would be -- from the floating-point save area, bias included, for a
  // floating element, and from the integer save area for an integer one.
  // Reading the pair as one object would take a _Complex double from the
  // integer save area instead.  gcc's alpha_gimplify_va_arg_1 recurses on the
  // element type twice for the same reason.
  if (!IsIndirect)
    if (const auto *CT = Ty->getAs<ComplexType>()) {
      QualType ET = CT->getElementType();
      llvm::Value *Real = Builder.CreateLoad(emitSlot(ET), "ap.real");
      llvm::Value *Imag = Builder.CreateLoad(emitSlot(ET), "ap.imag");
      return RValue::getComplex(Real, Imag);
    }

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
