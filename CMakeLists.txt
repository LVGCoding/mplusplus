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

# --- Executable -------------------------------------------------------------

add_executable(mplusplus
  src/main.cpp
  src/ui/font.cpp
  src/ui/file_dialog.cpp
  src/editor/editor.cpp
  src/ui/layout.cpp
  src/editor/input.cpp
  src/editor/file.cpp
  src/ui/clay_impl.cpp
  src/ui/clay_renderer_raylib.cpp
  src/math/nanosvg_impl.cpp
  src/math/latex_renderer.cpp
  src/math/svg_preprocessor.cpp 
)
target_include_directories(mplusplus PRIVATE src)
target_link_libraries(mplusplus PRIVATE raylib clay nfd)

if(MSVC)
  # /Zc:preprocessor gives MSVC a standard-conforming preprocessor, which
  # clay's variadic macros need.
  target_compile_options(mplusplus PRIVATE /W4 /Zc:preprocessor /utf-8)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT mplusplus)
else()
  # Clay layouts deliberately leave most config fields default-initialized.
  target_compile_options(mplusplus PRIVATE -Wall -Wextra
    -Wno-missing-field-initializers)
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
