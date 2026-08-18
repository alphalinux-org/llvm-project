//===-- RegisterContextPOSIX_alpha.cpp --------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <cstdint>

#include "lldb/Target/Thread.h"
#include "lldb/Utility/RegisterValue.h"

#include "RegisterContextPOSIX_alpha.h"

using namespace lldb_private;
using namespace lldb;

static const uint32_t g_gpr_regnums_alpha[] = {
    gpr_r0_alpha,       gpr_r1_alpha,  gpr_r2_alpha,  gpr_r3_alpha,
    gpr_r4_alpha,       gpr_r5_alpha,  gpr_r6_alpha,  gpr_r7_alpha,
    gpr_r8_alpha,       gpr_r9_alpha,  gpr_r10_alpha, gpr_r11_alpha,
    gpr_r12_alpha,      gpr_r13_alpha, gpr_r14_alpha, gpr_r15_alpha,
    gpr_r16_alpha,      gpr_r17_alpha, gpr_r18_alpha, gpr_r19_alpha,
    gpr_r20_alpha,      gpr_r21_alpha, gpr_r22_alpha, gpr_r23_alpha,
    gpr_r24_alpha,      gpr_r25_alpha, gpr_r26_alpha, gpr_r27_alpha,
    gpr_r28_alpha,      gpr_r29_alpha, gpr_r30_alpha, gpr_pc_alpha,
    gpr_unique_alpha,
    LLDB_INVALID_REGNUM // register sets need to end with this flag
};
static_assert((sizeof(g_gpr_regnums_alpha) / sizeof(g_gpr_regnums_alpha[0])) -
                      1 ==
                  k_num_gpr_registers_alpha,
              "g_gpr_regnums_alpha has wrong number of register infos");

static const uint32_t g_fpu_regnums_alpha[] = {
    fpu_f0_alpha,       fpu_f1_alpha,  fpu_f2_alpha,  fpu_f3_alpha,
    fpu_f4_alpha,       fpu_f5_alpha,  fpu_f6_alpha,  fpu_f7_alpha,
    fpu_f8_alpha,       fpu_f9_alpha,  fpu_f10_alpha, fpu_f11_alpha,
    fpu_f12_alpha,      fpu_f13_alpha, fpu_f14_alpha, fpu_f15_alpha,
    fpu_f16_alpha,      fpu_f17_alpha, fpu_f18_alpha, fpu_f19_alpha,
    fpu_f20_alpha,      fpu_f21_alpha, fpu_f22_alpha, fpu_f23_alpha,
    fpu_f24_alpha,      fpu_f25_alpha, fpu_f26_alpha, fpu_f27_alpha,
    fpu_f28_alpha,      fpu_f29_alpha, fpu_f30_alpha, fpu_fpcr_alpha,
    LLDB_INVALID_REGNUM // register sets need to end with this flag
};
static_assert((sizeof(g_fpu_regnums_alpha) / sizeof(g_fpu_regnums_alpha[0])) -
                      1 ==
                  k_num_fpr_registers_alpha,
              "g_fpu_regnums_alpha has wrong number of register infos");

enum { k_num_register_sets = 2 };

static const RegisterSet g_reg_sets_alpha[k_num_register_sets] = {
    {"General Purpose Registers", "gpr", k_num_gpr_registers_alpha,
     g_gpr_regnums_alpha},
    {"Floating Point Registers", "fpr", k_num_fpr_registers_alpha,
     g_fpu_regnums_alpha},
};

bool RegisterContextPOSIX_alpha::IsGPR(unsigned reg) {
  return reg <= k_last_gpr_alpha; // GPRs come first.
}

bool RegisterContextPOSIX_alpha::IsFPR(unsigned reg) {
  return k_first_fpr_alpha <= reg && reg <= k_last_fpr_alpha;
}

RegisterContextPOSIX_alpha::RegisterContextPOSIX_alpha(
    Thread &thread, uint32_t concrete_frame_idx,
    RegisterInfoInterface *register_info)
    : RegisterContext(thread, concrete_frame_idx) {
  m_register_info_up.reset(register_info);
}

RegisterContextPOSIX_alpha::~RegisterContextPOSIX_alpha() = default;

void RegisterContextPOSIX_alpha::Invalidate() {}

void RegisterContextPOSIX_alpha::InvalidateAllRegisters() {}

const RegisterInfo *RegisterContextPOSIX_alpha::GetRegisterInfo() {
  return m_register_info_up->GetRegisterInfo();
}

const RegisterInfo *
RegisterContextPOSIX_alpha::GetRegisterInfoAtIndex(size_t reg) {
  if (reg < k_num_registers_alpha)
    return &GetRegisterInfo()[reg];
  return nullptr;
}

size_t RegisterContextPOSIX_alpha::GetRegisterCount() {
  return k_num_registers_alpha;
}

unsigned RegisterContextPOSIX_alpha::GetRegisterOffset(unsigned reg) {
  assert(reg < k_num_registers_alpha && "Invalid register number.");
  return GetRegisterInfo()[reg].byte_offset;
}

unsigned RegisterContextPOSIX_alpha::GetRegisterSize(unsigned reg) {
  assert(reg < k_num_registers_alpha && "Invalid register number.");
  return GetRegisterInfo()[reg].byte_size;
}

const char *RegisterContextPOSIX_alpha::GetRegisterName(unsigned reg) {
  assert(reg < k_num_registers_alpha && "Invalid register offset.");
  return GetRegisterInfo()[reg].name;
}

size_t RegisterContextPOSIX_alpha::GetRegisterSetCount() {
  return k_num_register_sets;
}

const RegisterSet *RegisterContextPOSIX_alpha::GetRegisterSet(size_t set) {
  if (set < k_num_register_sets)
    return &g_reg_sets_alpha[set];
  return nullptr;
}
