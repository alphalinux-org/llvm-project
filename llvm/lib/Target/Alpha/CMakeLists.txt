add_llvm_component_group(Alpha)

set(LLVM_TARGET_DEFINITIONS Alpha.td)

tablegen(LLVM AlphaGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM AlphaGenCallingConv.inc -gen-callingconv)
tablegen(LLVM AlphaGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM AlphaGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM AlphaGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM AlphaGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM AlphaGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM AlphaGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(AlphaCommonTableGen)

add_llvm_target(AlphaCodeGen
  AlphaAsmPrinter.cpp
  AlphaExpandAtomicPseudo.cpp
  AlphaFrameLowering.cpp
  AlphaInstrInfo.cpp
  AlphaISelDAGToDAG.cpp
  AlphaISelLowering.cpp
  AlphaRegisterInfo.cpp
  AlphaSubtarget.cpp
  AlphaTargetMachine.cpp

  LINK_COMPONENTS
  AlphaDesc
  AlphaInfo
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  MC
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  Alpha
  )

add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
