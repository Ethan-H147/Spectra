# CMake generated Testfile for 
# Source directory: C:/Users/huyus/Projects/C-Spectra
# Build directory: C:/Users/huyus/Projects/C-Spectra/build-windows
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[dsp_tests]=] "C:/Users/huyus/Projects/C-Spectra/build-windows/Debug/dsp_tests.exe")
  set_tests_properties([=[dsp_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;77;add_test;C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[dsp_tests]=] "C:/Users/huyus/Projects/C-Spectra/build-windows/Release/dsp_tests.exe")
  set_tests_properties([=[dsp_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;77;add_test;C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[dsp_tests]=] "C:/Users/huyus/Projects/C-Spectra/build-windows/MinSizeRel/dsp_tests.exe")
  set_tests_properties([=[dsp_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;77;add_test;C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[dsp_tests]=] "C:/Users/huyus/Projects/C-Spectra/build-windows/RelWithDebInfo/dsp_tests.exe")
  set_tests_properties([=[dsp_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;77;add_test;C:/Users/huyus/Projects/C-Spectra/CMakeLists.txt;0;")
else()
  add_test([=[dsp_tests]=] NOT_AVAILABLE)
endif()
