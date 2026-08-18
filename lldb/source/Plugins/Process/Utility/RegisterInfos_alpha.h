//===-- RegisterInfos_alpha.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <cstddef>

#include "llvm/Support/Compiler.h"

#ifdef DECLARE_REGISTER_INFOS_ALPHA_STRUCT

// A core dump note holds 33 quadwords: $0 through $29 in order, then the user
// stack pointer ($30), the pc, and the thread pointer.  $31 reads as zero and
// is not stored.  See dump_elf_thread() in the kernel.
#define GPR_OFFSET(idx) ((idx) * 8)
#define GPR_SIZE (33 * 8)
// The floating-point note holds $f0 through $f30 and then the control register
// in the slot for $f31.
#define FPR_OFFSET(idx) (GPR_SIZE + (idx) * 8)

// The DWARF numbering (which is also GCC's DEBUGGER_REGNO) gives 0-30 to the
// general registers and 32-62 to the floating-point ones; 63 is the slot for
// $f31, used here for the control register.  Unwind information names the
// return address register ($26) rather than the pc, but the pc still needs a
// number for a frame's rule to be applied to it: 64 is what GCC's alternate
// return column and GDB both use, and 66 is GDB's number for the thread
// pointer.
#define dwarf_gpr(num) (num)
#define dwarf_fpr(num) (32 + (num))

// RegisterKind: EHFrame, DWARF, Generic, Process Plugin, LLDB

#define DEFINE_GPR(name, num, alt, generic)                                    \
  {                                                                            \
      #name,                                                                   \
      alt,                                                                     \
      8,                                                                       \
      GPR_OFFSET(num),                                                         \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {dwarf_gpr(num), dwarf_gpr(num), generic, LLDB_INVALID_REGNUM,           \
       gpr_##name##_alpha},                                                    \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_GPR_DWARF(name, num, offset, alt, generic)                      \
  {                                                                            \
      #name,                                                                   \
      alt,                                                                     \
      8,                                                                       \
      offset,                                                                  \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {num, num, generic, LLDB_INVALID_REGNUM, gpr_##name##_alpha},            \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_FPR(name, num)                                                  \
  {                                                                            \
      #name,                                                                   \
      nullptr,                                                                 \
      8,                                                                       \
      FPR_OFFSET(num),                                                         \
      lldb::eEncodingIEEE754,                                                  \
      lldb::eFormatFloat,                                                      \
      {dwarf_fpr(num), dwarf_fpr(num), LLDB_INVALID_REGNUM,                    \
       LLDB_INVALID_REGNUM, fpu_##name##_alpha},                               \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

#define DEFINE_FPCR()                                                          \
  {                                                                            \
      "fpcr",                                                                  \
      nullptr,                                                                 \
      8,                                                                       \
      FPR_OFFSET(31),                                                          \
      lldb::eEncodingUint,                                                     \
      lldb::eFormatHex,                                                        \
      {dwarf_fpr(31), dwarf_fpr(31), LLDB_INVALID_REGNUM, LLDB_INVALID_REGNUM, \
       fpu_fpcr_alpha},                                                        \
      nullptr,                                                                 \
      nullptr,                                                                 \
      nullptr,                                                                 \
  }

static lldb_private::RegisterInfo g_register_infos_alpha[] = {
    DEFINE_GPR(r0, 0, "v0", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r1, 1, "t0", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r2, 2, "t1", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r3, 3, "t2", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r4, 4, "t3", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r5, 5, "t4", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r6, 6, "t5", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r7, 7, "t6", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r8, 8, "t7", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r9, 9, "s0", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r10, 10, "s1", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r11, 11, "s2", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r12, 12, "s3", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r13, 13, "s4", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r14, 14, "s5", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r15, 15, "fp", LLDB_REGNUM_GENERIC_FP),
    DEFINE_GPR(r16, 16, "a0", LLDB_REGNUM_GENERIC_ARG1),
    DEFINE_GPR(r17, 17, "a1", LLDB_REGNUM_GENERIC_ARG2),
    DEFINE_GPR(r18, 18, "a2", LLDB_REGNUM_GENERIC_ARG3),
    DEFINE_GPR(r19, 19, "a3", LLDB_REGNUM_GENERIC_ARG4),
    DEFINE_GPR(r20, 20, "a4", LLDB_REGNUM_GENERIC_ARG5),
    DEFINE_GPR(r21, 21, "a5", LLDB_REGNUM_GENERIC_ARG6),
    DEFINE_GPR(r22, 22, "t8", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r23, 23, "t9", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r24, 24, "t10", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r25, 25, "t11", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r26, 26, "ra", LLDB_REGNUM_GENERIC_RA),
    DEFINE_GPR(r27, 27, "pv", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r28, 28, "at", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r29, 29, "gp", LLDB_INVALID_REGNUM),
    DEFINE_GPR(r30, 30, "sp", LLDB_REGNUM_GENERIC_SP),
    DEFINE_GPR_DWARF(pc, 64, GPR_OFFSET(31), nullptr, LLDB_REGNUM_GENERIC_PC),
    DEFINE_GPR_DWARF(unique, 66, GPR_OFFSET(32), "tp", LLDB_INVALID_REGNUM),

    DEFINE_FPR(f0, 0),
    DEFINE_FPR(f1, 1),
    DEFINE_FPR(f2, 2),
    DEFINE_FPR(f3, 3),
    DEFINE_FPR(f4, 4),
    DEFINE_FPR(f5, 5),
    DEFINE_FPR(f6, 6),
    DEFINE_FPR(f7, 7),
    DEFINE_FPR(f8, 8),
    DEFINE_FPR(f9, 9),
    DEFINE_FPR(f10, 10),
    DEFINE_FPR(f11, 11),
    DEFINE_FPR(f12, 12),
    DEFINE_FPR(f13, 13),
    DEFINE_FPR(f14, 14),
    DEFINE_FPR(f15, 15),
    DEFINE_FPR(f16, 16),
    DEFINE_FPR(f17, 17),
    DEFINE_FPR(f18, 18),
    DEFINE_FPR(f19, 19),
    DEFINE_FPR(f20, 20),
    DEFINE_FPR(f21, 21),
    DEFINE_FPR(f22, 22),
    DEFINE_FPR(f23, 23),
    DEFINE_FPR(f24, 24),
    DEFINE_FPR(f25, 25),
    DEFINE_FPR(f26, 26),
    DEFINE_FPR(f27, 27),
    DEFINE_FPR(f28, 28),
    DEFINE_FPR(f29, 29),
    DEFINE_FPR(f30, 30),
    DEFINE_FPCR(),
};

static_assert((sizeof(g_register_infos_alpha) /
               sizeof(g_register_infos_alpha[0])) == k_num_registers_alpha,
              "g_register_infos_alpha has wrong number of register infos");

#undef GPR_OFFSET
#undef GPR_SIZE
#undef FPR_OFFSET
#undef dwarf_gpr
#undef dwarf_fpr
#undef DEFINE_GPR
#undef DEFINE_GPR_DWARF
#undef DEFINE_FPR
#undef DEFINE_FPCR

#endif // DECLARE_REGISTER_INFOS_ALPHA_STRUCT
