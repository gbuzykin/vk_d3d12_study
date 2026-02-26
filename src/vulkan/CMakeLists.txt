file(GLOB_RECURSE VULKAN_HAL_SRC_FILES *.h;*.cpp)

add_library(app3d-vulkan SHARED ${VULKAN_HAL_SRC_FILES})

target_include_directories(app3d-vulkan PRIVATE $ENV{VULKAN_SDK}/include)
target_link_libraries(app3d-vulkan PRIVATE app3d-utils)

install(
  TARGETS app3d-vulkan
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
install_win32_pdb(app3d-vulkan)

add_compile_defs_and_include_dirs_targets(app3d-vulkan
                                          ${CMAKE_CURRENT_SOURCE_DIR})
