//===-- Alpha.td - Describe the Alpha Target Machine -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

include "AlphaFeatures.td"
include "AlphaRegisterInfo.td"
include "AlphaCallingConv.td"
include "AlphaInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GPRC>;

def AlphaInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Alpha processors.  CPU names and features follow GCC's table; feature sets
// are cumulative across EV generations.
//===----------------------------------------------------------------------===//

class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

def : Proc<"generic", []>;
def : Proc<"ev4",     []>;
def : Proc<"ev45",    []>;
def : Proc<"ev5",     []>;
def : Proc<"ev56",    [FeatureBWX]>;
def : Proc<"pca56",   [FeatureBWX, FeatureMVI]>;
def : Proc<"ev6",     [FeatureBWX, FeatureMVI, FeatureFIX]>;
def : Proc<"ev67",    [FeatureBWX, FeatureMVI, FeatureFIX, FeatureCIX]>;

//===----------------------------------------------------------------------===//
// Target declaration.
//===----------------------------------------------------------------------===//

def AlphaInstPrinter : AsmWriter {
  string AsmWriterClassName = "InstPrinter";
}

def Alpha : Target {
  let InstructionSet = AlphaInstrInfo;
  let AssemblyWriters = [AlphaInstPrinter];
}
