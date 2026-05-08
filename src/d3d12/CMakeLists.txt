file(GLOB_RECURSE REL_D3D12_SRC_FILES *.h;*.cpp)

add_library(
  app3d-rel-d3d12 SHARED
  ${CMAKE_SOURCE_DIR}/3rdparty/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp
  ${REL_D3D12_SRC_FILES})

target_include_directories(
  app3d-rel-d3d12
  PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/D3D12MemoryAllocator/include)
target_link_libraries(app3d-rel-d3d12 PRIVATE app3d-rel d3d12.lib dxgi.lib
                                              dxguid.lib)

install(
  TARGETS app3d-rel-d3d12
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT binaries
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
install_win32_pdb(app3d-rel-d3d12)

add_compile_defs_and_include_dirs_targets(app3d-rel-d3d12
                                          ${CMAKE_CURRENT_SOURCE_DIR})
