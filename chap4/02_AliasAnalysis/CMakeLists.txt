cmake_minimum_required(VERSION 3.5)

SET(CMAKE_C_COMPILER /usr/lib/llvm-10/bin/clang)
SET(CMAKE_CXX_COMPILER /usr/lib/llvm-10/bin/clang++)
SET(LLVM_SRC_DIR /usr/lib/llvm-10/)

include_directories(${LLVM_SRC_DIR}/include)

add_library(AliasAnalysis MODULE AliasAnalysis.cpp)
target_compile_features(AliasAnalysis PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(AliasAnalysis PROPERTIES COMPILE_FLAGS "-fno-rtti")
