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

add_executable(cpictures WIN32
  src/main.cpp
  src/app/app.cpp
  src/image/wic_decoder.cpp
  src/render/d2d_renderer.cpp
  src/viewer/viewer_window.cpp
)
target_compile_features(cpictures PRIVATE cxx_std_20)
target_include_directories(cpictures PRIVATE include src)
target_compile_definitions(cpictures PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(cpictures PRIVATE cpictures_core windowscodecs d2d1 dwrite dxgi ole32 shell32 user32 gdi32)

include(CTest)
if(BUILD_TESTING AND CPICTURES_BUILD_TESTS)
  add_executable(cpictures_core_tests tests/core_tests.cpp)
  target_compile_features(cpictures_core_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_core_tests PRIVATE include)
  target_link_libraries(cpictures_core_tests PRIVATE cpictures_core)
  add_test(NAME cpictures_core_tests COMMAND cpictures_core_tests)

  add_executable(cpictures_wic_tests tests/wic_tests.cpp src/image/wic_decoder.cpp)
  target_compile_features(cpictures_wic_tests PRIVATE cxx_std_20)
  target_include_directories(cpictures_wic_tests PRIVATE include src)
  target_compile_definitions(cpictures_wic_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(cpictures_wic_tests PRIVATE cpictures_core windowscodecs ole32)
  add_test(NAME cpictures_wic_tests COMMAND cpictures_wic_tests)
endif()
