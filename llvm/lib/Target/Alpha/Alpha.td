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
include "AlphaSchedule.td"

defm : RemapAllTargetPseudoPointerOperands<GPRC>;

def AlphaInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// Alpha processors.  CPU names and features follow GCC's table; feature sets
// are cumulative across EV generations.
//===----------------------------------------------------------------------===//

class Proc<string Name, list<SubtargetFeature> Features>
    : Processor<Name, NoItineraries, Features>;

// The out-of-order generations use the 21264 model; the generic CPU defaults to
// it.  The in-order ones get models of their own below.
def : Proc<"ev4",     []>;
def : Proc<"ev45",    []>;
def : ProcessorModel<"ev5",     Alpha21164Model, []>;
def : ProcessorModel<"ev56",    Alpha21164Model, [FeatureBWX]>;
def : ProcessorModel<"pca56",   Alpha21164Model, [FeatureBWX, FeatureMVI]>;
def : ProcessorModel<"generic", Alpha21264Model, []>;
def : ProcessorModel<"ev6",     Alpha21264Model, [FeatureBWX, FeatureMVI, FeatureFIX]>;
def : ProcessorModel<"ev67",    Alpha21264Model,
                     [FeatureBWX, FeatureMVI, FeatureFIX, FeatureCIX]>;

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
