file(GLOB_RECURSE REL_INCLUDE_FILES ${APP3D_INCLUDE_DIR}/rel/*.h)
file(GLOB_RECURSE REL_SRC_FILES *.h;*.cpp)

add_library(app3d-rel SHARED ${REL_INCLUDE_FILES} ${REL_SRC_FILES})

generate_export_header(
  app3d-rel EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/rel/config.h
  INCLUDE_GUARD_NAME APP3D_REL_H)

target_include_directories(app3d-rel PRIVATE $ENV{VULKAN_SDK}/include)
target_link_libraries(app3d-rel PUBLIC app3d-common)

install(
  TARGETS app3d-rel
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
install_win32_pdb(app3d-rel)

add_compile_defs_and_include_dirs_targets(app3d-rel ${CMAKE_CURRENT_SOURCE_DIR})
