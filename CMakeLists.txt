cmake_minimum_required(VERSION 3.20)
project(mplusplus LANGUAGES C CXX)

# Clay's CLAY({...}) macros rely on designated initializers, which need C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies -----------------------------------------------------------

include(FetchContent)

set(RAYLIB_VERSION 5.5)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)

# Use a system raylib if one matching the version is installed, otherwise
# download and build it from source.
find_package(raylib ${RAYLIB_VERSION} QUIET)
if(NOT raylib_FOUND)
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(raylib)
endif()

FetchContent_Declare(
  nativefiledialog
  URL https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v1.2.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nativefiledialog)

# Clay is a single-header library vendored in src/lib.
add_library(clay INTERFACE)
target_include_directories(clay SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)

# --- Warnings ---------------------------------------------------------------

function(mplusplus_set_warnings target)
  if(MSVC)
    # /Zc:preprocessor gives MSVC a standard-conforming preprocessor, which
    # clay's variadic macros need.
    target_compile_options(${target} PRIVATE /W4 /Zc:preprocessor /utf-8)
  else()
    # Clay layouts deliberately leave most config fields default-initialized.
    target_compile_options(${target} PRIVATE -Wall -Wextra
      -Wno-missing-field-initializers)
  endif()
endfunction()

# --- Core library -----------------------------------------------------------
# The parser, math nodes, and file types (including how each file type renders
# and handles input). Both the app and the tests link against this. It links
# raylib, but tests never open a window as long as they don't call raylib.

add_library(mplusplus_core STATIC
  src/editor/editor.cpp
  src/editor/fileBase.cpp
  src/editor/fileText.cpp
  src/editor/fileTextView.cpp
  src/editor/fileMpp.cpp
  src/editor/fileMppView.cpp
  src/math/FracNode.cpp
  src/math/GreekNode.cpp
  src/math/LineNode.cpp
  src/math/MatrixNode.cpp
  src/math/SqrtNode.cpp
  src/math/TextNode.cpp
  src/parser/parser.cpp
  src/ui/clay_impl.cpp
  src/ui/clay_renderer_raylib.cpp
)
target_include_directories(mplusplus_core PUBLIC src)
target_link_libraries(mplusplus_core PUBLIC raylib clay)
mplusplus_set_warnings(mplusplus_core)

# --- Executable -------------------------------------------------------------

add_executable(mplusplus
  src/main.cpp
  src/ui/font.cpp
  src/ui/file_dialog.cpp
  src/ui/layout.cpp
  src/math/nanosvg_impl.cpp
  src/math/latex_renderer.cpp
  src/math/svg_preprocessor.cpp
)
target_link_libraries(mplusplus PRIVATE mplusplus_core nfd)
mplusplus_set_warnings(mplusplus)

if(MSVC)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT mplusplus)
endif()

if(APPLE)
  target_link_libraries(mplusplus PRIVATE
    "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Copy assets next to the binary so the app can load them at runtime.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
  add_custom_command(TARGET mplusplus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:mplusplus>/assets)
endif()

# --- Tests ------------------------------------------------------------------
# Run with: ctest --test-dir build --output-on-failure   (or ./test.sh)

option(MPLUSPLUS_BUILD_TESTS "Build the M++ unit tests" ON)

if(MPLUSPLUS_BUILD_TESTS)
  enable_testing()

  # Use a system GoogleTest if installed, otherwise download it.
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
  endif()

  # New test files: drop a *_test.cpp file into tests/ and re-run CMake.
  file(GLOB MPLUSPLUS_TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*_test.cpp)

  add_executable(mplusplus_tests ${MPLUSPLUS_TEST_SOURCES})
  target_include_directories(mplusplus_tests PRIVATE tests)
  target_link_libraries(mplusplus_tests PRIVATE mplusplus_core GTest::gtest_main)
  mplusplus_set_warnings(mplusplus_tests)

  include(GoogleTest)
  gtest_discover_tests(mplusplus_tests)
endif()
