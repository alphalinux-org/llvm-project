// RUN: %clang_cc1 -std=c17 -fsyntax-only -verify=ext -Wno-unused %s
// RUN: %clang_cc1 -std=c2x -fsyntax-only -verify=compat -Wpre-c2x-compat -Wno-unused %s
// RUN: %clang_cc1 -fsyntax-only -verify=cpp -Wbit-int-extension -Wno-unused -x c++ %s
//
// gnu proves the extension warning fires in a GNU C mode, gnuquiet that it is
// silent without -Wbit-int-extension.
// RUN: %clang_cc1 -std=gnu17 -fsyntax-only -verify=gnu -Wbit-int-extension -Wno-unused %s
// RUN: %clang_cc1 -std=gnu17 -fsyntax-only -verify=gnuquiet -Wno-unused %s

#if 18446744073709551615uwb // ext-warning {{'_BitInt' suffix for literals is a C23 extension}} \
                               compat-warning {{'_BitInt' suffix for literals is incompatible with C standards before C23}} \
                               gnu-warning {{'_BitInt' suffix for literals is a Clang extension}} \
                               cpp-error {{invalid suffix 'uwb' on integer constant}}
#endif

#if 18446744073709551615__uwb // ext-error {{invalid suffix '__uwb' on integer constant}} \
                               compat-error {{invalid suffix '__uwb' on integer constant}} \
                               gnu-error {{invalid suffix '__uwb' on integer constant}} \
                               gnuquiet-error {{invalid suffix '__uwb' on integer constant}} \
                               cpp-warning {{'_BitInt' suffix for literals is a Clang extension}}
#endif

void func(void) {
  18446744073709551615wb; // ext-warning {{'_BitInt' suffix for literals is a C23 extension}} \
                             compat-warning {{'_BitInt' suffix for literals is incompatible with C standards before C23}} \
                             gnu-warning {{'_BitInt' suffix for literals is a Clang extension}} \
                             cpp-error {{invalid suffix 'wb' on integer constant}}

  18446744073709551615__wb; // ext-error {{invalid suffix '__wb' on integer constant}} \
                             compat-error {{invalid suffix '__wb' on integer constant}} \
                             gnu-error {{invalid suffix '__wb' on integer constant}} \
                             gnuquiet-error {{invalid suffix '__wb' on integer constant}} \
                             cpp-warning {{'_BitInt' suffix for literals is a Clang extension}}
}
