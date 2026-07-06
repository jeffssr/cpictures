cmake_minimum_required(VERSION 4.2)
project(cpictures VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPICTURES_BUILD_TESTS "Build cpictures tests" ON)

add_library(cpictures_core STATIC
  src/image/image_document.cpp
  src/image/image_list.cpp
  src/image/supported_formats.cpp
  src/platform/path_utils.cpp
  src/platform/window_metrics.cpp
)
target_compile_features(cpictures_core PUBLIC cxx_std_20)
target_include_directories(cpictures_core PUBLIC include)
target_compile_definitions(cpictures_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
  target_compile_options(cpictures_core PRIVATE /FS)
endif()

include(CTest)
if(BUILD_TESTING AND CPICTURES_BUILD_TESTS)
  add_executable(cpictures_core_tests tests/core_tests.cpp)
  target_compile_features(cpictures_core_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_core_tests PRIVATE include)
  target_link_libraries(cpictures_core_tests PRIVATE cpictures_core)
  add_test(NAME cpictures_core_tests COMMAND cpictures_core_tests)
endif()
