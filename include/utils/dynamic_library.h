#pragma once

#include <filesystem>
#include <string_view>

namespace app3d {

void* loadDynamicLibrary(const std::filesystem::path& library_dir, std::string_view library_name);
void freeDynamicLibrary(void* library);
void* getDynamicLibraryEntry(void* library, const char* entry_name);

}  // namespace app3d
