#pragma once

#include "common/config.h"

#include <filesystem>
#include <string_view>

namespace app3d {

APP3D_COMMON_EXPORT void* loadDynamicLibrary(const std::filesystem::path& library_dir, std::string_view library_name);
APP3D_COMMON_EXPORT void freeDynamicLibrary(void* library);
APP3D_COMMON_EXPORT void* getDynamicLibraryEntry(void* library, const char* entry_name);

}  // namespace app3d
