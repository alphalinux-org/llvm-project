//===-- RegisterContextPOSIXCore_alpha.cpp ------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RegisterContextPOSIXCore_alpha.h"

#include "lldb/Utility/DataBufferHeap.h"
#include "lldb/Utility/RegisterValue.h"

using namespace lldb_private;

RegisterContextCorePOSIX_alpha::RegisterContextCorePOSIX_alpha(
    Thread &thread, RegisterInfoInterface *register_info,
    const DataExtractor &gpregset, llvm::ArrayRef<CoreNote> notes)
    : RegisterContextPOSIX_alpha(thread, 0, register_info) {
  m_gpr_buffer = std::make_shared<DataBufferHeap>(gpregset.GetDataStart(),
                                                  gpregset.GetByteSize());
  m_gpr.SetData(m_gpr_buffer);
  m_gpr.SetByteOrder(gpregset.GetByteOrder());

  DataExtractor fpregset = getRegset(
      notes, register_info->GetTargetArchitecture().GetTriple(), FPR_Desc);
  m_fpr_buffer = std::make_shared<DataBufferHeap>(fpregset.GetDataStart(),
                                                  fpregset.GetByteSize());
  m_fpr.SetData(m_fpr_buffer);
  m_fpr.SetByteOrder(fpregset.GetByteOrder());
}

RegisterContextCorePOSIX_alpha::~RegisterContextCorePOSIX_alpha() = default;

bool RegisterContextCorePOSIX_alpha::ReadGPR() { return true; }

bool RegisterContextCorePOSIX_alpha::ReadFPR() { return true; }

bool RegisterContextCorePOSIX_alpha::WriteGPR() {
  assert(false && "Writing registers is not allowed for core dumps");
  return false;
}

bool RegisterContextCorePOSIX_alpha::WriteFPR() {
  assert(false && "Writing registers is not allowed for core dumps");
  return false;
}

bool RegisterContextCorePOSIX_alpha::ReadRegister(const RegisterInfo *reg_info,
                                                  RegisterValue &value) {
  const uint32_t reg = reg_info->kinds[lldb::eRegisterKindLLDB];
  if (reg == LLDB_INVALID_REGNUM)
    return false;

  // $31 and $f31 read as zero and are not stored, so the two notes hold the
  // registers this context describes and nothing else.  The floating-point
  // offsets follow the general ones in the register info, so take the note size
  // back off to index into the floating-point note.
  if (IsGPR(reg)) {
    lldb::offset_t offset = reg_info->byte_offset;
    uint64_t v = m_gpr.GetMaxU64(&offset, reg_info->byte_size);
    if (offset == reg_info->byte_offset + reg_info->byte_size) {
      value.SetUInt(v, reg_info->byte_size);
      return true;
    }
  }

  if (IsFPR(reg)) {
    lldb::offset_t offset =
        reg_info->byte_offset - m_register_info_up->GetGPRSize();
    lldb::offset_t start = offset;
    uint64_t v = m_fpr.GetMaxU64(&offset, reg_info->byte_size);
    if (offset == start + reg_info->byte_size) {
      value.SetUInt(v, reg_info->byte_size);
      return true;
    }
  }

  return false;
}

bool RegisterContextCorePOSIX_alpha::ReadAllRegisterValues(
    lldb::WritableDataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_alpha::WriteRegister(const RegisterInfo *reg_info,
                                                   const RegisterValue &value) {
  return false;
}

bool RegisterContextCorePOSIX_alpha::WriteAllRegisterValues(
    const lldb::DataBufferSP &data_sp) {
  return false;
}

bool RegisterContextCorePOSIX_alpha::HardwareSingleStep(bool enable) {
  return false;
}
