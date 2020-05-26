cmake_minimum_required(VERSION 3.5)

SET(CMAKE_C_COMPILER /usr/local/llvm-5.0/bin/clang)
SET(CMAKE_CXX_COMPILER /usr/local/llvm-5.0/bin/clang++)
SET(LLVM_SRC_DIR /usr/local/llvm-5.0/)

include_directories(
    ${LLVM_SRC_DIR}/include
)

add_library(InstCount MODULE InstCount.cpp)
target_compile_features(InstCount PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(InstCount PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)
