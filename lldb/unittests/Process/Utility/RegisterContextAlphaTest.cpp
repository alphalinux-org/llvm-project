//===-- RegisterContextAlphaTest.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "Plugins/Process/Utility/RegisterContextLinux_alpha.h"
#include "Plugins/Process/Utility/lldb-alpha-register-enums.h"
#include "lldb/Utility/ArchSpec.h"
#include "lldb/lldb-defines.h"

using namespace lldb_private;

static const RegisterInfo &GetInfo(const RegisterContextLinux_alpha &Ctx,
                                   uint32_t Reg) {
  return Ctx.GetRegisterInfo()[Reg];
}

// The offsets are those of the notes a core dump carries, which the kernel
// writes in dump_elf_thread(): $0 through $29, then the user stack pointer, the
// pc and the thread pointer.
TEST(RegisterContextAlphaTest, CoreNoteOffsets) {
  RegisterContextLinux_alpha Ctx{ArchSpec("alpha-unknown-linux-gnu")};

  EXPECT_EQ(GetInfo(Ctx, gpr_r0_alpha).byte_offset, 0u);
  EXPECT_EQ(GetInfo(Ctx, gpr_r29_alpha).byte_offset, 29u * 8);
  EXPECT_EQ(GetInfo(Ctx, gpr_r30_alpha).byte_offset, 30u * 8);
  EXPECT_EQ(GetInfo(Ctx, gpr_pc_alpha).byte_offset, 31u * 8);
  EXPECT_EQ(GetInfo(Ctx, gpr_unique_alpha).byte_offset, 32u * 8);
  EXPECT_EQ(Ctx.GetGPRSize(), 33u * 8);

  // The floating-point note follows the general one, and the control register
  // sits in the slot $f31 would occupy.
  EXPECT_EQ(GetInfo(Ctx, fpu_f0_alpha).byte_offset, Ctx.GetGPRSize());
  EXPECT_EQ(GetInfo(Ctx, fpu_fpcr_alpha).byte_offset,
            Ctx.GetGPRSize() + 31 * 8);
}

// GCC's DEBUGGER_REGNO: 0-30 for the general registers, 32-62 for the
// floating-point ones.  The pc needs a number of its own for a frame's unwind
// rule to be applied to it, and 64 is what GCC's alternate return column and
// GDB both use; without it a backtrace stops at the first frame.
TEST(RegisterContextAlphaTest, DwarfNumbering) {
  RegisterContextLinux_alpha Ctx{ArchSpec("alpha-unknown-linux-gnu")};

  EXPECT_EQ(GetInfo(Ctx, gpr_r0_alpha).kinds[lldb::eRegisterKindDWARF], 0u);
  EXPECT_EQ(GetInfo(Ctx, gpr_r26_alpha).kinds[lldb::eRegisterKindDWARF], 26u);
  EXPECT_EQ(GetInfo(Ctx, gpr_r30_alpha).kinds[lldb::eRegisterKindDWARF], 30u);
  EXPECT_EQ(GetInfo(Ctx, gpr_pc_alpha).kinds[lldb::eRegisterKindDWARF], 64u);
  EXPECT_EQ(GetInfo(Ctx, fpu_f0_alpha).kinds[lldb::eRegisterKindDWARF], 32u);
  EXPECT_EQ(GetInfo(Ctx, fpu_f30_alpha).kinds[lldb::eRegisterKindDWARF], 62u);
}

// The return address is in $26 and the stack pointer in $30; the unwinder
// reaches for both by their generic numbers.
TEST(RegisterContextAlphaTest, GenericRegisters) {
  RegisterContextLinux_alpha Ctx{ArchSpec("alpha-unknown-linux-gnu")};

  EXPECT_EQ(GetInfo(Ctx, gpr_r26_alpha).kinds[lldb::eRegisterKindGeneric],
            static_cast<uint32_t>(LLDB_REGNUM_GENERIC_RA));
  EXPECT_EQ(GetInfo(Ctx, gpr_r30_alpha).kinds[lldb::eRegisterKindGeneric],
            static_cast<uint32_t>(LLDB_REGNUM_GENERIC_SP));
  EXPECT_EQ(GetInfo(Ctx, gpr_r15_alpha).kinds[lldb::eRegisterKindGeneric],
            static_cast<uint32_t>(LLDB_REGNUM_GENERIC_FP));
  EXPECT_EQ(GetInfo(Ctx, gpr_pc_alpha).kinds[lldb::eRegisterKindGeneric],
            static_cast<uint32_t>(LLDB_REGNUM_GENERIC_PC));
  EXPECT_EQ(GetInfo(Ctx, gpr_r16_alpha).kinds[lldb::eRegisterKindGeneric],
            static_cast<uint32_t>(LLDB_REGNUM_GENERIC_ARG1));
}
