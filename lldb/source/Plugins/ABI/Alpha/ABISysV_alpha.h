//===-- ABISysV_alpha.h -----------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_ABI_ALPHA_ABISYSV_ALPHA_H
#define LLDB_SOURCE_PLUGINS_ABI_ALPHA_ABISYSV_ALPHA_H

#include "lldb/Target/ABI.h"
#include "lldb/lldb-private.h"

class ABISysV_alpha : public lldb_private::RegInfoBasedABI {
public:
  ~ABISysV_alpha() override = default;

  size_t GetRedZoneSize() const override;

  bool PrepareTrivialCall(lldb_private::Thread &thread, lldb::addr_t sp,
                          lldb::addr_t functionAddress,
                          lldb::addr_t returnAddress,
                          llvm::ArrayRef<lldb::addr_t> args) const override;

  bool GetArgumentValues(lldb_private::Thread &thread,
                         lldb_private::ValueList &values) const override;

  lldb_private::Status
  SetReturnValueObject(lldb::StackFrameSP &frame_sp,
                       lldb::ValueObjectSP &new_value) override;

  lldb::ValueObjectSP
  GetReturnValueObjectImpl(lldb_private::Thread &thread,
                           lldb_private::CompilerType &type) const override;

  lldb::UnwindPlanSP CreateFunctionEntryUnwindPlan() override;

  lldb::UnwindPlanSP CreateDefaultUnwindPlan() override;

  bool RegisterIsVolatile(const lldb_private::RegisterInfo *reg_info) override;

  bool CallFrameAddressIsValid(lldb::addr_t cfa) override {
    // The stack pointer is kept 16-byte aligned.
    if (cfa & 0xfull)
      return false;
    return cfa != 0;
  }

  bool CodeAddressIsValid(lldb::addr_t pc) override {
    // Every instruction is a naturally aligned longword.
    return (pc & 3ull) == 0;
  }

  const lldb_private::RegisterInfo *
  GetRegisterInfoArray(uint32_t &count) override;

  // Static Functions

  static void Initialize();

  static void Terminate();

  static lldb::ABISP CreateInstance(lldb::ProcessSP process_sp,
                                    const lldb_private::ArchSpec &arch);

  static llvm::StringRef GetPluginNameStatic() { return "sysv-alpha"; }

  // PluginInterface protocol

  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

protected:
  lldb::ValueObjectSP
  GetReturnValueObjectSimple(lldb_private::Thread &thread,
                             lldb_private::CompilerType &ast_type) const;

  bool RegisterIsCalleeSaved(const lldb_private::RegisterInfo *reg_info);

private:
  using lldb_private::RegInfoBasedABI::RegInfoBasedABI;
};

#endif // LLDB_SOURCE_PLUGINS_ABI_ALPHA_ABISYSV_ALPHA_H
