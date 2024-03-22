# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Jeron/esp/v5.2.1/esp-idf/components/bootloader/subproject"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/tmp"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/src/bootloader-stamp"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/src"
  "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Work/CSC2106/jeron's side/df_server/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
