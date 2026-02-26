cmake_minimum_required(VERSION 3.21)

project(
  App3D
  DESCRIPTION "Project for studying Vulkan and D3D12"
  LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

option(OPTION_DEBUG_USE_SANITIZERS "Use Sanitizers for Debug build" ON)

set(INSTALL_WIN32_SYMBOLS_DIR ${CMAKE_INSTALL_BINDIR})

set(CMAKE_CXX_STANDARD
    20
    CACHE STRING "Default C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message("Using C++${CMAKE_CXX_STANDARD} standard")

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(UNIX)
  set(CMAKE_SKIP_RPATH OFF)
  set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
  set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

# ##############################################################################
# Compilation flags

if(MSVC)
  add_compile_options(/Zc:__cplusplus /utf-8)
else()
  add_compile_options(-Wall -pedantic)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options($<$<CONFIG:Debug>:-fstandalone-debug>)
  endif()
endif()

if(OPTION_DEBUG_USE_SANITIZERS)
  message("Using sanitizers for Debug build")
  if(MSVC)
    add_compile_options($<$<CONFIG:Debug>:/fsanitize=address>)
  else()
    add_compile_options($<$<CONFIG:Debug>:-fsanitize=address,undefined,leak>)
    add_link_options($<$<CONFIG:Debug>:-fsanitize=address,undefined,leak>)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      add_link_options($<$<CONFIG:Debug>:-shared-libsan>)
    endif()
  endif()
endif()

# ##############################################################################
# Add `uxs` build target

block()
set(UXS_USE_ZLIB OFF)
set(UXS_USE_LIBZIP OFF)
set(UXS_DO_INSTALL_SDK OFF)
FetchContent_Declare(uxs SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/uxs)
FetchContent_MakeAvailable(uxs)
endblock()

# ##############################################################################
# Add `app3d` build target

if(WIN32)
  set(APP3D_USE_PLATFORM WIN32)
elseif(UNIX)
  set(APP3D_USE_PLATFORM XCB)
endif()

set(APP3D_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${APP3D_INCLUDE_DIR})

add_compile_definitions(APP3D_USE_PLATFORM_${APP3D_USE_PLATFORM})

add_subdirectory(src/utils)

file(GLOB_RECURSE APP3D_SRC_FILES src/app3d/*.h;src/app3d/*.cpp)

if(APP3D_USE_PLATFORM STREQUAL "WIN32")
  file(GLOB_RECURSE APP3D_PLATFORM_SRC_FILES
       src/platform/win32/*.h;src/platform/win32/*.cpp)
elseif(APP3D_USE_PLATFORM STREQUAL "XCB")
  file(GLOB_RECURSE APP3D_PLATFORM_SRC_FILES
       src/platform/xcb/*.h;src/platform/xcb/*.cpp)
endif()

add_executable(app3d ${APP3D_SRC_FILES} ${APP3D_PLATFORM_SRC_FILES})

target_link_libraries(app3d PRIVATE app3d-utils)

if(APP3D_USE_PLATFORM STREQUAL "XCB")
  target_link_libraries(app3d PRIVATE xcb)
endif()

install(TARGETS app3d RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                              COMPONENT binaries)
install_win32_pdb(app3d)

# ##############################################################################
# Auxiliary

add_compile_defs_and_include_dirs_targets(app3d ${CMAKE_CURRENT_SOURCE_DIR})
