add_llvm_component_library(LLVMAlphaDesc
  AlphaELFObjectWriter.cpp
  AlphaInstPrinter.cpp
  AlphaMCAsmInfo.cpp
  AlphaMCCodeEmitter.cpp
  AlphaMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  Support
  AlphaInfo

  ADD_TO_COMPONENT
  Alpha
  )
