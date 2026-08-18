add_lldb_unittest(ABIAlphaTests
  ABIAlphaTest.cpp

  LINK_COMPONENTS
    Support
    Alpha
  LINK_LIBS
    lldbTarget
    lldbPluginABIAlpha
  )
