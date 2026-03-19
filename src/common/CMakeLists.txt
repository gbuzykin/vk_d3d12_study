file(GLOB_RECURSE UTILS_INCLUDE_FILES ${APP3D_INCLUDE_DIR}/common/*.h)
file(GLOB_RECURSE UTILS_SRC_FILES *.h;*.cpp)

add_library(app3d-common SHARED ${UTILS_INCLUDE_FILES} ${UTILS_SRC_FILES})

generate_export_header(
  app3d-common EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/generated/common/config.h
  INCLUDE_GUARD_NAME APP3D_COMMON_H)

target_link_libraries(app3d-common PUBLIC UXS::UXS)

install(
  TARGETS app3d-common
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
install_win32_pdb(app3d-common)

add_compile_defs_and_include_dirs_targets(app3d-common
                                          ${CMAKE_CURRENT_SOURCE_DIR})
