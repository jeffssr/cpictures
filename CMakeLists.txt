cmake_minimum_required(VERSION 3.24)
project(cpictures VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPICTURES_BUILD_TESTS "Build cpictures tests" ON)

include(CTest)
if(BUILD_TESTING AND CPICTURES_BUILD_TESTS)
  add_executable(cpictures_core_tests tests/core_tests.cpp)
  target_compile_features(cpictures_core_tests PRIVATE cxx_std_20)
  add_test(NAME cpictures_core_tests COMMAND cpictures_core_tests)
endif()
