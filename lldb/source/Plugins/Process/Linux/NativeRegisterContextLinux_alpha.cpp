//===-- NativeRegisterContextLinux_alpha.cpp ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#if defined(__alpha__) && defined(__linux__)

#include "NativeRegisterContextLinux_alpha.h"
#include "Plugins/Process/Linux/NativeProcessLinux.h"
#include "Plugins/Process/Utility/RegisterContextLinux_alpha.h"

#include "lldb/Host/HostInfo.h"
#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/RegisterValue.h"
#include "lldb/Utility/Status.h"

#include <sys/ptrace.h>

using namespace lldb_private;
using namespace lldb_private::process_linux;

// Alpha has no request that moves a whole register set: ptrace names one
// register at a time, numbered as arch/alpha/kernel/ptrace.c has them.  $31 and
// $f31 read as zero and are left out of that numbering, which is why the
// floating-point registers start at 32 and the pc sits above them.
enum {
  ptrace_r0_alpha = 0,
  ptrace_sp_alpha = 30,
  ptrace_f0_alpha = 32,
  ptrace_fpcr_alpha = 63,
  ptrace_pc_alpha = 64,
  ptrace_unique_alpha = 65,
};

static llvm::Expected<unsigned> GetPtraceRegNum(uint32_t lldb_regnum) {
  if (lldb_regnum >= gpr_r0_alpha && lldb_regnum <= gpr_r30_alpha)
    return ptrace_r0_alpha + (lldb_regnum - gpr_r0_alpha);
  if (lldb_regnum == gpr_pc_alpha)
    return ptrace_pc_alpha;
  if (lldb_regnum == gpr_unique_alpha)
    return ptrace_unique_alpha;
  if (lldb_regnum >= fpu_f0_alpha && lldb_regnum <= fpu_f30_alpha)
    return ptrace_f0_alpha + (lldb_regnum - fpu_f0_alpha);
  if (lldb_regnum == fpu_fpcr_alpha)
    return ptrace_fpcr_alpha;
  return llvm::createStringError("no ptrace number for register %" PRIu32,
                                 lldb_regnum);
}

std::unique_ptr<NativeRegisterContextLinux>
NativeRegisterContextLinux::CreateHostNativeRegisterContextLinux(
    const ArchSpec &target_arch, NativeThreadLinux &native_thread) {
  return std::make_unique<NativeRegisterContextLinux_alpha>(target_arch,
                                                            native_thread);
}

llvm::Expected<ArchSpec>
NativeRegisterContextLinux::DetermineArchitecture(lldb::tid_t tid) {
  return HostInfo::GetArchitecture();
}

NativeRegisterContextLinux_alpha::NativeRegisterContextLinux_alpha(
    const ArchSpec &target_arch, NativeThreadProtocol &native_thread)
    : NativeRegisterContextRegisterInfo(
          native_thread, new RegisterContextLinux_alpha(target_arch)),
      NativeRegisterContextLinux(native_thread) {}

// The general-purpose set is $0-$30, the pc and the thread pointer; the
// floating-point one is $f0-$f30 and the control register.
static const uint32_t g_gpr_regnums[] = {
    gpr_r0_alpha,  gpr_r1_alpha,  gpr_r2_alpha,    gpr_r3_alpha,  gpr_r4_alpha,
    gpr_r5_alpha,  gpr_r6_alpha,  gpr_r7_alpha,    gpr_r8_alpha,  gpr_r9_alpha,
    gpr_r10_alpha, gpr_r11_alpha, gpr_r12_alpha,   gpr_r13_alpha, gpr_r14_alpha,
    gpr_r15_alpha, gpr_r16_alpha, gpr_r17_alpha,   gpr_r18_alpha, gpr_r19_alpha,
    gpr_r20_alpha, gpr_r21_alpha, gpr_r22_alpha,   gpr_r23_alpha, gpr_r24_alpha,
    gpr_r25_alpha, gpr_r26_alpha, gpr_r27_alpha,   gpr_r28_alpha, gpr_r29_alpha,
    gpr_r30_alpha, gpr_pc_alpha,  gpr_unique_alpha};

static const uint32_t g_fpu_regnums[] = {
    fpu_f0_alpha,  fpu_f1_alpha,  fpu_f2_alpha,  fpu_f3_alpha,  fpu_f4_alpha,
    fpu_f5_alpha,  fpu_f6_alpha,  fpu_f7_alpha,  fpu_f8_alpha,  fpu_f9_alpha,
    fpu_f10_alpha, fpu_f11_alpha, fpu_f12_alpha, fpu_f13_alpha, fpu_f14_alpha,
    fpu_f15_alpha, fpu_f16_alpha, fpu_f17_alpha, fpu_f18_alpha, fpu_f19_alpha,
    fpu_f20_alpha, fpu_f21_alpha, fpu_f22_alpha, fpu_f23_alpha, fpu_f24_alpha,
    fpu_f25_alpha, fpu_f26_alpha, fpu_f27_alpha, fpu_f28_alpha, fpu_f29_alpha,
    fpu_f30_alpha, fpu_fpcr_alpha};

static const RegisterSet g_reg_sets[] = {
    {"General Purpose Registers", "gpr", std::size(g_gpr_regnums),
     g_gpr_regnums},
    {"Floating Point Registers", "fpr", std::size(g_fpu_regnums),
     g_fpu_regnums},
};

uint32_t NativeRegisterContextLinux_alpha::GetRegisterSetCount() const {
  return std::size(g_reg_sets);
}

const RegisterSet *
NativeRegisterContextLinux_alpha::GetRegisterSet(uint32_t set_index) const {
  if (set_index < std::size(g_reg_sets))
    return &g_reg_sets[set_index];
  return nullptr;
}

uint32_t NativeRegisterContextLinux_alpha::GetUserRegisterCount() const {
  return GetRegisterInfoInterface().GetUserRegisterCount();
}

Status
NativeRegisterContextLinux_alpha::ReadRegister(const RegisterInfo *reg_info,
                                               RegisterValue &reg_value) {
  if (!reg_info)
    return Status::FromErrorString("reg_info is null");

  uint32_t reg = reg_info->kinds[lldb::eRegisterKindLLDB];
  if (reg == LLDB_INVALID_REGNUM)
    return Status::FromErrorStringWithFormat("register \"%s\" is not available",
                                             reg_info->name);

  auto PtraceReg = GetPtraceRegNum(reg);
  if (!PtraceReg)
    return Status::FromError(PtraceReg.takeError());

  long value = 0;
  Status error = NativeProcessLinux::PtraceWrapper(
      PTRACE_PEEKUSER, m_thread.GetID(),
      reinterpret_cast<void *>(static_cast<uintptr_t>(*PtraceReg)), nullptr, 0,
      &value);
  if (error.Fail())
    return error;

  reg_value.SetUInt64(static_cast<uint64_t>(value));
  return Status();
}

Status NativeRegisterContextLinux_alpha::WriteRegister(
    const RegisterInfo *reg_info, const RegisterValue &reg_value) {
  if (!reg_info)
    return Status::FromErrorString("reg_info is null");

  uint32_t reg = reg_info->kinds[lldb::eRegisterKindLLDB];
  if (reg == LLDB_INVALID_REGNUM)
    return Status::FromErrorStringWithFormat("register \"%s\" is not available",
                                             reg_info->name);

  auto PtraceReg = GetPtraceRegNum(reg);
  if (!PtraceReg)
    return Status::FromError(PtraceReg.takeError());

  uint64_t value = reg_value.GetAsUInt64();
  return NativeProcessLinux::PtraceWrapper(
      PTRACE_POKEUSER, m_thread.GetID(),
      reinterpret_cast<void *>(static_cast<uintptr_t>(*PtraceReg)),
      reinterpret_cast<void *>(static_cast<uintptr_t>(value)));
}

Status NativeRegisterContextLinux_alpha::ReadAllRegisterValues(
    lldb::WritableDataBufferSP &data_sp) {
  const uint32_t count = GetRegisterInfoInterface().GetRegisterCount();
  data_sp.reset(new DataBufferHeap(count * sizeof(uint64_t), 0));
  uint64_t *dst = reinterpret_cast<uint64_t *>(data_sp->GetBytes());

  for (uint32_t i = 0; i < count; ++i) {
    RegisterValue value;
    Status error = ReadRegister(GetRegisterInfoAtIndex(i), value);
    if (error.Fail())
      return error;
    dst[i] = value.GetAsUInt64();
  }
  return Status();
}

Status NativeRegisterContextLinux_alpha::WriteAllRegisterValues(
    const lldb::DataBufferSP &data_sp) {
  const uint32_t count = GetRegisterInfoInterface().GetRegisterCount();
  if (!data_sp || data_sp->GetByteSize() < count * sizeof(uint64_t))
    return Status::FromErrorString("register state buffer is too small");

  const uint64_t *src = reinterpret_cast<const uint64_t *>(data_sp->GetBytes());
  for (uint32_t i = 0; i < count; ++i) {
    RegisterValue value(src[i]);
    Status error = WriteRegister(GetRegisterInfoAtIndex(i), value);
    if (error.Fail())
      return error;
  }
  return Status();
}

#endif // defined(__alpha__) && defined(__linux__)
