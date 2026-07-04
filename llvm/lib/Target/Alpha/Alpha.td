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

// Each generation uses its own scheduling model; the generic CPU defaults to
// the EV6 model.
def : ProcessorModel<"generic", Alpha21264Model, []>;
def : ProcessorModel<"ev4",     Alpha21064Model, []>;
def : ProcessorModel<"ev45",    Alpha21064Model, []>;
def : ProcessorModel<"ev5",     Alpha21164Model, []>;
def : ProcessorModel<"ev56",    Alpha21164Model, [FeatureBWX]>;
def : ProcessorModel<"pca56",   Alpha21164Model, [FeatureBWX, FeatureMVI]>;
def : ProcessorModel<"ev6",     Alpha21264Model,
                     [FeatureBWX, FeatureMVI, FeatureFIX, FeaturePrefetch]>;
def : ProcessorModel<"ev67",    Alpha21264Model,
                     [FeatureBWX, FeatureMVI, FeatureFIX, FeatureCIX,
                      FeaturePrefetch]>;

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
