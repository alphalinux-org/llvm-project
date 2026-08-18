//===-- ABIAlphaTest.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/ABI/Alpha/ABISysV_alpha.h"
#include "Plugins/Process/Utility/lldb-alpha-register-enums.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/Utility/ArchSpec.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "gtest/gtest.h"

using namespace lldb_private;
using namespace lldb;

class ABIAlphaTest : public testing::Test {
public:
  static void SetUpTestCase() {
    LLVMInitializeAlphaTargetInfo();
    LLVMInitializeAlphaTargetMC();
    ABISysV_alpha::Initialize();
  }

  static void TearDownTestCase() {
    ABISysV_alpha::Terminate();
    llvm::llvm_shutdown();
  }

protected:
  static ABISP GetABI() {
    return ABI::FindPlugin(ProcessSP(), ArchSpec("alpha-unknown-linux-gnu"));
  }

  // A register the ABI can be asked about is one it can find an LLDB register
  // number on; nothing else about it matters here.
  static RegisterInfo MakeInfo(uint32_t reg) {
    RegisterInfo info = {};
    for (uint32_t &kind : info.kinds)
      kind = LLDB_INVALID_REGNUM;
    info.kinds[eRegisterKindLLDB] = reg;
    return info;
  }

  static bool IsVolatile(const ABISP &abi, uint32_t reg) {
    RegisterInfo info = MakeInfo(reg);
    return abi->RegisterIsVolatile(&info);
  }
};

// $9-$15 and $f2-$f9 are preserved across a call, as is the stack pointer, and
// $26 is treated as preserved so that a frame can unwind through it.  $29 is
// not: the ELF global pointer is caller-saved, which is why a call is followed
// by an ldgp.  Reading a variable live in $29 out of a caller's frame after
// its callee has reloaded it gives the callee's value.
TEST_F(ABIAlphaTest, RegisterIsVolatile) {
  ABISP abi = GetABI();
  ASSERT_TRUE(abi);

  EXPECT_TRUE(IsVolatile(abi, gpr_r29_alpha));

  for (uint32_t reg = gpr_r9_alpha; reg <= gpr_r15_alpha; ++reg)
    EXPECT_FALSE(IsVolatile(abi, reg)) << "$" << reg - gpr_r0_alpha;
  for (uint32_t reg = fpu_f2_alpha; reg <= fpu_f9_alpha; ++reg)
    EXPECT_FALSE(IsVolatile(abi, reg)) << "$f" << reg - fpu_f0_alpha;
  EXPECT_FALSE(IsVolatile(abi, gpr_r26_alpha));
  EXPECT_FALSE(IsVolatile(abi, gpr_r30_alpha));

  // The argument, return value and temporary registers are not.
  EXPECT_TRUE(IsVolatile(abi, gpr_r0_alpha));
  EXPECT_TRUE(IsVolatile(abi, gpr_r16_alpha));
  EXPECT_TRUE(IsVolatile(abi, gpr_r27_alpha));
  EXPECT_TRUE(IsVolatile(abi, fpu_f0_alpha));
  EXPECT_TRUE(IsVolatile(abi, fpu_f10_alpha));
}

// Both plans describe a frame that has not been set up: the canonical frame
// address is the stack pointer itself and the return address is still in $26.
TEST_F(ABIAlphaTest, UnwindPlans) {
  ABISP abi = GetABI();
  ASSERT_TRUE(abi);

  for (UnwindPlanSP plan :
       {abi->CreateFunctionEntryUnwindPlan(), abi->CreateDefaultUnwindPlan()}) {
    ASSERT_TRUE(plan);
    EXPECT_EQ(plan->GetRegisterKind(), eRegisterKindLLDB);
    ASSERT_TRUE(plan->IsValidRowIndex(0));
    const UnwindPlan::Row *row = plan->GetRowAtIndex(0);
    ASSERT_TRUE(row);

    EXPECT_EQ(row->GetCFAValue().GetRegisterNumber(),
              static_cast<uint32_t>(gpr_r30_alpha));
    EXPECT_EQ(row->GetCFAValue().GetOffset(), 0);

    UnwindPlan::Row::AbstractRegisterLocation pc;
    ASSERT_TRUE(row->GetRegisterInfo(gpr_pc_alpha, pc));
    EXPECT_TRUE(pc.IsInOtherRegister());
    EXPECT_EQ(pc.GetRegisterNumber(), static_cast<uint32_t>(gpr_r26_alpha));
  }

  // RegisterContextUnwind asserts on a default plan that does not say what
  // becomes of the registers it does not name.  Only the default plan is used
  // that way, and only it has to say so.
  EXPECT_TRUE(abi->CreateDefaultUnwindPlan()
                  ->GetRowAtIndex(0)
                  ->GetUnspecifiedRegistersAreUndefined());
}
