//===-- ABISysV_alpha.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABISysV_alpha.h"

#include "llvm/TargetParser/Triple.h"

#include "lldb/Core/PluginManager.h"
#include "lldb/Core/Value.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StackFrame.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/Utility/Status.h"
#include "lldb/ValueObject/ValueObjectConstResult.h"
#include "lldb/ValueObject/ValueObjectMemory.h"
#include "lldb/ValueObject/ValueObjectRegister.h"

#include "Plugins/Process/Utility/lldb-alpha-register-enums.h"

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE_ADV(ABISysV_alpha, ABIAlpha)

#define DECLARE_REGISTER_INFOS_ALPHA_STRUCT
#include "Plugins/Process/Utility/RegisterInfos_alpha.h"
#undef DECLARE_REGISTER_INFOS_ALPHA_STRUCT

// The first six arguments travel in $16-$21 (or $f16-$f21 for a floating-point
// one); the rest go on the stack.  A trivial call passes integers only.
static const uint32_t g_arg_regs[] = {gpr_r16_alpha, gpr_r17_alpha,
                                      gpr_r18_alpha, gpr_r19_alpha,
                                      gpr_r20_alpha, gpr_r21_alpha};

size_t ABISysV_alpha::GetRedZoneSize() const { return 0; }

ABISP ABISysV_alpha::CreateInstance(ProcessSP process_sp,
                                    const ArchSpec &arch) {
  if (arch.GetTriple().getArch() != llvm::Triple::alpha)
    return ABISP();
  return ABISP(
      new ABISysV_alpha(std::move(process_sp), MakeMCRegisterInfo(arch)));
}

bool ABISysV_alpha::PrepareTrivialCall(Thread &thread, addr_t sp,
                                       addr_t func_addr, addr_t return_addr,
                                       llvm::ArrayRef<addr_t> args) const {
  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return false;

  if (args.size() > std::size(g_arg_regs))
    return false;

  for (size_t i = 0; i < args.size(); ++i)
    if (!reg_ctx->WriteRegisterFromUnsigned(
            reg_ctx->GetRegisterInfoAtIndex(g_arg_regs[i]), args[i]))
      return false;

  // The stack pointer stays 16-byte aligned.
  sp &= ~0xfull;

  // A call enters through the procedure value in $27, which the callee also
  // uses to form its own $gp, and leaves the return address in $26.
  const RegisterInfo *pc_info = reg_ctx->GetRegisterInfoAtIndex(gpr_pc_alpha);
  const RegisterInfo *sp_info = reg_ctx->GetRegisterInfoAtIndex(gpr_r30_alpha);
  const RegisterInfo *ra_info = reg_ctx->GetRegisterInfoAtIndex(gpr_r26_alpha);
  const RegisterInfo *pv_info = reg_ctx->GetRegisterInfoAtIndex(gpr_r27_alpha);

  return reg_ctx->WriteRegisterFromUnsigned(ra_info, return_addr) &&
         reg_ctx->WriteRegisterFromUnsigned(sp_info, sp) &&
         reg_ctx->WriteRegisterFromUnsigned(pv_info, func_addr) &&
         reg_ctx->WriteRegisterFromUnsigned(pc_info, func_addr);
}

static bool ReadIntegerArgument(Scalar &scalar, unsigned bit_width,
                                bool is_signed, Thread &thread,
                                uint32_t *arg_reg_idx, addr_t &sp) {
  if (bit_width > 64)
    return false; // Scalar can't hold large integer arguments yet.

  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  uint64_t raw;
  if (*arg_reg_idx < std::size(g_arg_regs)) {
    raw = reg_ctx->ReadRegisterAsUnsigned(
        reg_ctx->GetRegisterInfoAtIndex(g_arg_regs[*arg_reg_idx]), 0);
    ++(*arg_reg_idx);
  } else {
    Status error;
    if (!thread.GetProcess()->ReadMemory(sp, &raw, sizeof(raw), error))
      return false;
    sp += 8;
  }

  scalar = raw;
  if (is_signed)
    scalar.SignExtend(bit_width);
  return true;
}

bool ABISysV_alpha::GetArgumentValues(Thread &thread, ValueList &values) const {
  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return false;

  // Arguments beyond the sixth sit in the caller's outgoing argument area, at
  // the stack pointer.
  addr_t sp = reg_ctx->GetSP(0);
  if (!sp)
    return false;
  uint32_t arg_reg_idx = 0;

  for (uint32_t i = 0, e = values.GetSize(); i < e; ++i) {
    Value *value = values.GetValueAtIndex(i);
    if (!value)
      return false;

    CompilerType type = value->GetCompilerType();
    std::optional<uint64_t> bit_size =
        llvm::expectedToOptional(type.GetBitSize(&thread));
    if (!bit_size)
      return false;
    bool is_signed = false;
    if (type.IsIntegerOrEnumerationType(is_signed)) {
      if (!ReadIntegerArgument(value->GetScalar(), *bit_size, is_signed, thread,
                               &arg_reg_idx, sp))
        return false;
    } else if (type.IsPointerType()) {
      if (!ReadIntegerArgument(value->GetScalar(), *bit_size, false, thread,
                               &arg_reg_idx, sp))
        return false;
    } else {
      return false;
    }
  }
  return true;
}

Status ABISysV_alpha::SetReturnValueObject(StackFrameSP &frame_sp,
                                           ValueObjectSP &new_value_sp) {
  Status error;
  if (!new_value_sp) {
    error = Status::FromErrorString("Empty value object for return value.");
    return error;
  }

  CompilerType type = new_value_sp->GetCompilerType();
  if (!type) {
    error = Status::FromErrorString("Null clang type for return value.");
    return error;
  }

  RegisterContext *reg_ctx = frame_sp->GetThread()->GetRegisterContext().get();
  if (!reg_ctx) {
    error = Status::FromErrorString("No register context.");
    return error;
  }

  bool is_signed = false;
  if (type.IsIntegerOrEnumerationType(is_signed) || type.IsPointerType()) {
    // An integer or a pointer comes back in $0.
    DataExtractor data;
    Status data_error;
    if (new_value_sp->GetData(data, data_error) == 0) {
      error = Status::FromErrorStringWithFormat(
          "Couldn't convert return value to raw data: %s",
          data_error.AsCString());
      return error;
    }
    lldb::offset_t offset = 0;
    uint64_t raw = data.GetMaxU64(&offset, data.GetByteSize());
    if (!reg_ctx->WriteRegisterFromUnsigned(
            reg_ctx->GetRegisterInfoAtIndex(gpr_r0_alpha), raw))
      error = Status::FromErrorString("Couldn't write $0.");
  } else if (type.IsFloatingPointType()) {
    // A float or a double comes back in $f0.
    DataExtractor data;
    Status data_error;
    if (new_value_sp->GetData(data, data_error) == 0) {
      error = Status::FromErrorStringWithFormat(
          "Couldn't convert return value to raw data: %s",
          data_error.AsCString());
      return error;
    }
    std::optional<uint64_t> byte_size =
        llvm::expectedToOptional(type.GetByteSize(frame_sp.get()));
    if (!byte_size || (*byte_size != 4 && *byte_size != 8)) {
      error = Status::FromErrorString(
          "Only float and double are returned in $f0.");
      return error;
    }
    // A floating-point register always holds T_floating: the hardware widens
    // an S_floating value on the way in and narrows it on the way out, so a
    // float is written as the double it widens to.  Copying its four bytes
    // into the low half of $f0 would store a bit pattern that reads back as
    // something else entirely.
    lldb::offset_t offset = 0;
    double d = *byte_size == 4 ? static_cast<double>(data.GetFloat(&offset))
                               : data.GetDouble(&offset);
    RegisterValue value(d);
    const RegisterInfo *f0 = reg_ctx->GetRegisterInfoAtIndex(fpu_f0_alpha);
    if (!reg_ctx->WriteRegister(f0, value))
      error = Status::FromErrorString("Couldn't write $f0.");
  } else {
    error = Status::FromErrorString(
        "We don't support returning this type by value.");
  }

  return error;
}

ValueObjectSP
ABISysV_alpha::GetReturnValueObjectSimple(Thread &thread,
                                          CompilerType &type) const {
  ValueObjectSP return_valobj_sp;
  Value value;

  if (!type)
    return return_valobj_sp;

  value.SetCompilerType(type);

  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return return_valobj_sp;

  const uint32_t type_flags = type.GetTypeInfo();
  if (type_flags & eTypeIsScalar) {
    value.SetValueType(Value::ValueType::Scalar);

    if (type_flags & eTypeIsInteger) {
      // An integer comes back in $0, sign- or zero-extended to a quadword.
      uint64_t raw = reg_ctx->ReadRegisterAsUnsigned(
          reg_ctx->GetRegisterInfoAtIndex(gpr_r0_alpha), 0);
      std::optional<uint64_t> byte_size =
          llvm::expectedToOptional(type.GetByteSize(&thread));
      if (!byte_size)
        return return_valobj_sp;
      bool is_signed = (type_flags & eTypeIsSigned) != 0;
      switch (*byte_size) {
      default:
        return return_valobj_sp;
      case 8:
        value.GetScalar() = is_signed ? (int64_t)raw : raw;
        break;
      case 4:
        value.GetScalar() = is_signed ? (int64_t)(int32_t)raw : (uint32_t)raw;
        break;
      case 2:
        value.GetScalar() = is_signed ? (int64_t)(int16_t)raw : (uint16_t)raw;
        break;
      case 1:
        value.GetScalar() = is_signed ? (int64_t)(int8_t)raw : (uint8_t)raw;
        break;
      }
    } else if (type_flags & eTypeIsFloat) {
      if (type_flags & eTypeIsComplex)
        return return_valobj_sp; // The two halves ride in $f0/$f1; not handled.

      std::optional<uint64_t> byte_size =
          llvm::expectedToOptional(type.GetByteSize(&thread));
      if (!byte_size || (*byte_size != 4 && *byte_size != 8))
        return return_valobj_sp; // long double comes back in memory.

      const RegisterInfo *f0 = reg_ctx->GetRegisterInfoAtIndex(fpu_f0_alpha);
      RegisterValue f0_value;
      if (!reg_ctx->ReadRegister(f0, f0_value))
        return return_valobj_sp;

      DataExtractor data;
      if (!f0_value.GetData(data))
        return return_valobj_sp;

      // $f0 holds T_floating whatever the value's type is -- an S_floating
      // load widens and a store narrows -- so a float is read as the double it
      // was widened to and narrowed here, not taken from the low four bytes.
      lldb::offset_t offset = 0;
      double d = data.GetDouble(&offset);
      if (*byte_size == 4)
        value.GetScalar() = static_cast<float>(d);
      else
        value.GetScalar() = d;
    } else {
      return return_valobj_sp;
    }
  } else {
    return return_valobj_sp;
  }

  return ValueObjectConstResult::Create(thread.GetStackFrameAtIndex(0).get(),
                                        value, ConstString(""));
}

ValueObjectSP
ABISysV_alpha::GetReturnValueObjectImpl(Thread &thread,
                                        CompilerType &type) const {
  ValueObjectSP return_valobj_sp;

  if (!type)
    return return_valobj_sp;

  return_valobj_sp = GetReturnValueObjectSimple(thread, type);
  if (return_valobj_sp)
    return return_valobj_sp;

  RegisterContext *reg_ctx = thread.GetRegisterContext().get();
  if (!reg_ctx)
    return return_valobj_sp;

  // Anything wider than a register is returned in memory: the caller hands the
  // callee a buffer in $16, and the callee returns that pointer in $0.  For
  // floating-point types that is X_floating (long double) and _Complex long
  // double only.  A float, a double, a _Complex float and a _Complex double all
  // come back in $f0 (and $f1 for the imaginary half), so $0 holds no address
  // for them and reading one out of it produces a value at a garbage location.
  // The two complex-in-register cases are not reconstructed here; returning
  // nothing says so, which is what GetReturnValueObjectSimple already does.
  uint32_t type_flags = type.GetTypeInfo();
  bool in_memory = type.IsAggregateType();
  if (!in_memory && (type_flags & eTypeIsFloat)) {
    std::optional<uint64_t> byte_size =
        llvm::expectedToOptional(type.GetByteSize(&thread));
    uint64_t reg_limit = (type_flags & eTypeIsComplex) ? 16 : 8;
    in_memory = byte_size && *byte_size > reg_limit;
  }
  if (in_memory) {
    addr_t storage_addr = reg_ctx->ReadRegisterAsUnsigned(
        reg_ctx->GetRegisterInfoAtIndex(gpr_r0_alpha), 0);
    return_valobj_sp = ValueObjectMemory::Create(
        &thread, "", Address(storage_addr, nullptr), type);
  }

  return return_valobj_sp;
}

UnwindPlanSP ABISysV_alpha::CreateFunctionEntryUnwindPlan() {
  UnwindPlan::Row row;

  // At the entry point the frame has not been set up yet, so the canonical
  // frame address is the stack pointer, and the return address is still in $26.
  row.GetCFAValue().SetIsRegisterPlusOffset(gpr_r30_alpha, 0);
  row.SetRegisterLocationToRegister(gpr_pc_alpha, gpr_r26_alpha, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindLLDB);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("alpha at-func-entry default");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanValidAtAllInstructions(eLazyBoolNo);
  plan_sp->SetUnwindPlanForSignalTrap(eLazyBoolNo);
  return plan_sp;
}

UnwindPlanSP ABISysV_alpha::CreateDefaultUnwindPlan() {
  // A frame keeps no chain pointer, so there is nothing to walk back through
  // without unwind information: take the frame to be in its pre-prologue state,
  // which holds for a leaf and for a frame a signal interrupted.  The backend
  // always emits .eh_frame, so only a module carrying no unwind information at
  // all reaches this.
  UnwindPlan::Row row;
  row.GetCFAValue().SetIsRegisterPlusOffset(gpr_r30_alpha, 0);
  // The plan speaks only for the registers it names; RegisterContextUnwind
  // requires a default plan to say so, and asserts on one that does not.
  row.SetUnspecifiedRegistersAreUndefined(true);
  row.SetRegisterLocationToRegister(gpr_pc_alpha, gpr_r26_alpha, true);

  auto plan_sp = std::make_shared<UnwindPlan>(eRegisterKindLLDB);
  plan_sp->AppendRow(std::move(row));
  plan_sp->SetSourceName("alpha default unwind plan");
  plan_sp->SetSourcedFromCompiler(eLazyBoolNo);
  plan_sp->SetUnwindPlanValidAtAllInstructions(eLazyBoolNo);
  plan_sp->SetUnwindPlanForSignalTrap(eLazyBoolNo);
  return plan_sp;
}

bool ABISysV_alpha::RegisterIsVolatile(const RegisterInfo *reg_info) {
  return !RegisterIsCalleeSaved(reg_info);
}

bool ABISysV_alpha::RegisterIsCalleeSaved(const RegisterInfo *reg_info) {
  if (!reg_info)
    return false;

  // $9-$15 and $f2-$f9 are preserved across a call, as is the stack pointer.
  // $26 holds the return address the frame needs to unwind, so it is treated
  // as preserved here too.  $29 is not: on ELF the global pointer is
  // caller-saved, which is why AlphaCallingConv.td's CSR_Alpha does not list
  // it and why a call is followed by an ldgp.
  switch (reg_info->kinds[eRegisterKindLLDB]) {
  case gpr_r9_alpha:
  case gpr_r10_alpha:
  case gpr_r11_alpha:
  case gpr_r12_alpha:
  case gpr_r13_alpha:
  case gpr_r14_alpha:
  case gpr_r15_alpha:
  case gpr_r26_alpha:
  case gpr_r30_alpha:
  case gpr_pc_alpha:
  case fpu_f2_alpha:
  case fpu_f3_alpha:
  case fpu_f4_alpha:
  case fpu_f5_alpha:
  case fpu_f6_alpha:
  case fpu_f7_alpha:
  case fpu_f8_alpha:
  case fpu_f9_alpha:
    return true;
  default:
    return false;
  }
}

const RegisterInfo *ABISysV_alpha::GetRegisterInfoArray(uint32_t &count) {
  count = k_num_registers_alpha;
  return g_register_infos_alpha;
}

void ABISysV_alpha::Initialize() {
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "System V ABI for Alpha targets", CreateInstance);
}

void ABISysV_alpha::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}
