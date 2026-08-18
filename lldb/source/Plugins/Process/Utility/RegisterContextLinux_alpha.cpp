//===-- RegisterContextLinux_alpha.cpp ---------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RegisterContextLinux_alpha.h"
#include "RegisterContextPOSIX_alpha.h"
#include "lldb/lldb-defines.h"

using namespace lldb_private;
using namespace lldb;

#define DECLARE_REGISTER_INFOS_ALPHA_STRUCT
#include "RegisterInfos_alpha.h"
#undef DECLARE_REGISTER_INFOS_ALPHA_STRUCT

RegisterContextLinux_alpha::RegisterContextLinux_alpha(
    const ArchSpec &target_arch)
    : lldb_private::RegisterInfoInterface(target_arch) {}

const RegisterInfo *RegisterContextLinux_alpha::GetRegisterInfo() const {
  return g_register_infos_alpha;
}

uint32_t RegisterContextLinux_alpha::GetRegisterCount() const {
  return k_num_registers_alpha;
}

uint32_t RegisterContextLinux_alpha::GetUserRegisterCount() const {
  return k_num_registers_alpha;
}

// The general-purpose note of a core dump: 33 quadwords.
size_t RegisterContextLinux_alpha::GetGPRSize() const { return 33 * 8; }
