add_llvm_component_library(LLVMAlphaDisassembler
  AlphaDisassembler.cpp

  LINK_COMPONENTS
  MCDisassembler
  MC
  AlphaInfo
  Support

  ADD_TO_COMPONENT
  Alpha
  )
