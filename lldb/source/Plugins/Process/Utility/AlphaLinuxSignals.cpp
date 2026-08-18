//===-- AlphaLinuxSignals.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AlphaLinuxSignals.h"

using namespace lldb_private;

// Alpha keeps the OSF/1 signal numbering rather than the one the other Linux
// ports share, so twelve of the first thirty-one signals sit at a different
// number: see arch/alpha/include/uapi/asm/signal.h.  It also has two signals
// the generic set does not, SIGEMT and SIGINFO, and lacks SIGSTKFLT and
// SIGPWR, whose numbers it uses for something else.
//
// The real-time range is not affected: SIGRTMIN is 34 and SIGRTMAX is 64 there
// as everywhere else, so everything from 32 up is inherited unchanged.
//
// Each signal added here also gets the sender codes (SI_USER, SI_QUEUE and the
// rest) that LinuxSignals attaches to every signal, since those describe who
// raised it rather than what it means.
#define ADD_SENDER_CODES(signo)                                                \
  AddSignalCode(signo, 0, "sent by kill, sigsend or raise",                    \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, 0x80, "sent by kernel (SI_KERNEL)",                     \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -1, "sent by sigqueue", SignalCodePrintOption::Sender); \
  AddSignalCode(signo, -2, "sent by timer expiration",                         \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -3, "sent by real time mesq state change",              \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -4, "sent by AIO completion",                           \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -5, "sent by queued SIGIO",                             \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -6, "sent by tkill system call",                        \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -7, "sent by execve() killing subsidiary threads",      \
                SignalCodePrintOption::Sender);                                \
  AddSignalCode(signo, -60, "sent by glibc async name lookup completion",      \
                SignalCodePrintOption::Sender)

#define ADD_ALPHA_SIGNAL(signo, name, ...)                                     \
  RemoveSignal(signo);                                                         \
  AddSignal(signo, name, __VA_ARGS__);                                         \
  ADD_SENDER_CODES(signo)

AlphaLinuxSignals::AlphaLinuxSignals() : LinuxSignals() { Reset(); }

void AlphaLinuxSignals::Reset() {
  LinuxSignals::Reset();

  // clang-format off
  //               SIGNO  NAME         SUPPRESS  STOP    NOTIFY  DESCRIPTION
  //               =====  ===========  ========  ======  ======  ==============================================
  ADD_ALPHA_SIGNAL(7,     "SIGEMT",    false,    true,   true,   "emulator trap");
  ADD_ALPHA_SIGNAL(10,    "SIGBUS",    false,    true,   true,   "bus error");
  ADD_ALPHA_SIGNAL(12,    "SIGSYS",    false,    true,   true,   "invalid system call");
  ADD_ALPHA_SIGNAL(16,    "SIGURG",    false,    true,   true,   "urgent data on socket");
  ADD_ALPHA_SIGNAL(17,    "SIGSTOP",   true,     true,   true,   "process stop");
  ADD_ALPHA_SIGNAL(18,    "SIGTSTP",   false,    true,   true,   "tty stop");
  ADD_ALPHA_SIGNAL(19,    "SIGCONT",   false,    false,  true,   "process continue");
  ADD_ALPHA_SIGNAL(20,    "SIGCHLD",   false,    false,  true,   "child status has changed", "SIGCLD");
  ADD_ALPHA_SIGNAL(23,    "SIGIO",     false,    true,   true,   "input/output ready/Pollable event", "SIGPOLL");
  ADD_ALPHA_SIGNAL(29,    "SIGINFO",   false,    true,   true,   "status request from keyboard");
  ADD_ALPHA_SIGNAL(30,    "SIGUSR1",   false,    true,   true,   "user defined signal 1");
  ADD_ALPHA_SIGNAL(31,    "SIGUSR2",   false,    true,   true,   "user defined signal 2");
  // clang-format on

  // SIGBUS is the one moved signal carrying codes of its own.
  AddSignalCode(10, 1, "illegal alignment", SignalCodePrintOption::Address);
  AddSignalCode(10, 2, "illegal address", SignalCodePrintOption::Address);
  AddSignalCode(10, 3, "hardware error", SignalCodePrintOption::Address);
}
