//===-- NativeRegisterContextLinux_alpha.h ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if defined(__alpha__) && defined(__linux__)

#ifndef lldb_NativeRegisterContextLinux_alpha_h
#define lldb_NativeRegisterContextLinux_alpha_h

#include "Plugins/Process/Linux/NativeRegisterContextLinux.h"
#include "Plugins/Process/Utility/lldb-alpha-register-enums.h"

namespace lldb_private {
namespace process_linux {

class NativeProcessLinux;

class NativeRegisterContextLinux_alpha : public NativeRegisterContextLinux {
public:
  NativeRegisterContextLinux_alpha(const ArchSpec &target_arch,
                                   NativeThreadProtocol &native_thread);

  uint32_t GetRegisterSetCount() const override;

  const RegisterSet *GetRegisterSet(uint32_t set_index) const override;

  uint32_t GetUserRegisterCount() const override;

  Status ReadRegister(const RegisterInfo *reg_info,
                      RegisterValue &reg_value) override;

  Status WriteRegister(const RegisterInfo *reg_info,
                       const RegisterValue &reg_value) override;

  Status ReadAllRegisterValues(lldb::WritableDataBufferSP &data_sp) override;

  Status WriteAllRegisterValues(const lldb::DataBufferSP &data_sp) override;

protected:
  // ptrace names one register at a time on Alpha, so there is no buffer for a
  // whole set to hand out.
  void *GetGPRBuffer() override { return nullptr; }
  void *GetFPRBuffer() override { return nullptr; }
  size_t GetFPRSize() override { return 0; }
};

} // namespace process_linux
} // namespace lldb_private

#endif // #ifndef lldb_NativeRegisterContextLinux_alpha_h

#endif // defined(__alpha__) && defined(__linux__)
