file(GLOB_RECURSE UTILS_INCLUDE_FILES ${APP3D_INCLUDE_DIR}/utils/*.h)
file(GLOB_RECURSE UTILS_SRC_FILES *.h;*.cpp)

add_library(app3d-utils STATIC ${UTILS_INCLUDE_FILES} ${UTILS_SRC_FILES})

set_target_properties(app3d-utils PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(app3d-utils PUBLIC UXS::UXS)

add_compile_defs_and_include_dirs_targets(app3d-utils
                                          ${CMAKE_CURRENT_SOURCE_DIR})
