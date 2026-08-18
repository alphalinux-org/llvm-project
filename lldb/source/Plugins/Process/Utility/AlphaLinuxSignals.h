//===-- AlphaLinuxSignals.h -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_ALPHALINUXSIGNALS_H
#define LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_ALPHALINUXSIGNALS_H

#include "LinuxSignals.h"

namespace lldb_private {

/// Linux signals as alpha numbers them.
class AlphaLinuxSignals : public LinuxSignals {
public:
  AlphaLinuxSignals();

private:
  void Reset() override;
};

} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_PROCESS_UTILITY_ALPHALINUXSIGNALS_H
