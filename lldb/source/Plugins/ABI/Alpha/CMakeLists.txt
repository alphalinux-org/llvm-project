add_lldb_library(lldbPluginABIAlpha PLUGIN
  ABISysV_alpha.cpp

  LINK_COMPONENTS
    Support
    TargetParser
  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbValueObject
  )
