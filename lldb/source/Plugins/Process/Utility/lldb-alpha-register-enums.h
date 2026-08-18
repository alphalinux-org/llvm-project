//===-- lldb-alpha-register-enums.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_LLDB_ALPHA_REGISTER_ENUMS_H
#define LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_LLDB_ALPHA_REGISTER_ENUMS_H

// LLDB register codes (e.g. RegisterKind == eRegisterKindLLDB)

// Internal codes for all alpha registers.
enum {
  k_first_gpr_alpha,
  gpr_r0_alpha = k_first_gpr_alpha,
  gpr_r1_alpha,
  gpr_r2_alpha,
  gpr_r3_alpha,
  gpr_r4_alpha,
  gpr_r5_alpha,
  gpr_r6_alpha,
  gpr_r7_alpha,
  gpr_r8_alpha,
  gpr_r9_alpha,
  gpr_r10_alpha,
  gpr_r11_alpha,
  gpr_r12_alpha,
  gpr_r13_alpha,
  gpr_r14_alpha,
  gpr_r15_alpha,
  gpr_r16_alpha,
  gpr_r17_alpha,
  gpr_r18_alpha,
  gpr_r19_alpha,
  gpr_r20_alpha,
  gpr_r21_alpha,
  gpr_r22_alpha,
  gpr_r23_alpha,
  gpr_r24_alpha,
  gpr_r25_alpha,
  gpr_r26_alpha,
  gpr_r27_alpha,
  gpr_r28_alpha,
  gpr_r29_alpha,
  gpr_r30_alpha,
  gpr_pc_alpha,
  // The thread pointer, which the kernel stores in the slot a core dump once
  // used for the processor status.
  gpr_unique_alpha,
  k_last_gpr_alpha = gpr_unique_alpha,

  k_first_fpr_alpha,
  fpu_f0_alpha = k_first_fpr_alpha,
  fpu_f1_alpha,
  fpu_f2_alpha,
  fpu_f3_alpha,
  fpu_f4_alpha,
  fpu_f5_alpha,
  fpu_f6_alpha,
  fpu_f7_alpha,
  fpu_f8_alpha,
  fpu_f9_alpha,
  fpu_f10_alpha,
  fpu_f11_alpha,
  fpu_f12_alpha,
  fpu_f13_alpha,
  fpu_f14_alpha,
  fpu_f15_alpha,
  fpu_f16_alpha,
  fpu_f17_alpha,
  fpu_f18_alpha,
  fpu_f19_alpha,
  fpu_f20_alpha,
  fpu_f21_alpha,
  fpu_f22_alpha,
  fpu_f23_alpha,
  fpu_f24_alpha,
  fpu_f25_alpha,
  fpu_f26_alpha,
  fpu_f27_alpha,
  fpu_f28_alpha,
  fpu_f29_alpha,
  fpu_f30_alpha,
  // $f31 reads as zero, so the slot it would occupy carries the floating-point
  // control register instead, both in a core dump and here.
  fpu_fpcr_alpha,
  k_last_fpr_alpha = fpu_fpcr_alpha,

  k_num_registers_alpha,
  k_num_gpr_registers_alpha = k_last_gpr_alpha - k_first_gpr_alpha + 1,
  k_num_fpr_registers_alpha = k_last_fpr_alpha - k_first_fpr_alpha + 1,
};

#endif // LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_LLDB_ALPHA_REGISTER_ENUMS_H
