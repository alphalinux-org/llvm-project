add_llvm_component_library(LLVMAlphaAsmParser
  AlphaAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  AlphaDesc
  AlphaInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  Alpha
  )
