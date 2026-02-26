#include "utils/dynamic_library.h"

#include "utils/print.h"

#include <uxs/string_util.h>

#include <string>

#if defined(WIN32)
#    include <windows.h>  // NOLINT
#elif defined(__linux__)
#    include <dlfcn.h>
#    include <unistd.h>
#endif

using namespace app3d;

void* app3d::loadDynamicLibrary(const std::filesystem::path& library_dir, std::string_view library_name) {
    std::filesystem::path library_path(uxs::utf_native_path_adapter{}(library_name));

#if defined(WIN32)
    library_path += ".dll";
#elif defined(__linux__)
    if (!library_name.starts_with("lib")) { library_path = "lib" + library_path.native(); }
    library_path += ".so";
#endif

    if (!library_dir.empty()) {
        library_path = library_dir / library_path;
#if defined(__linux__)
        if (!library_dir.is_absolute()) { library_path = "../lib/" / library_path; }
#endif
        library_path = std::filesystem::canonical(library_path);
    }

    std::string error_message;

#if defined(WIN32)
    void* library = ::LoadLibraryW(library_path.c_str());
    if (!library) {
        DWORD error_code = ::GetLastError();
        WCHAR* buffer = nullptr;
        try {
            ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (LPWSTR)&buffer, 0, nullptr);
            if (buffer) {
                error_message = uxs::from_wide_to_utf8(buffer);
                ::LocalFree(buffer);
            }
        } catch (...) {
            if (buffer) { ::LocalFree(buffer); }
            throw;
        }
    }
#elif defined(__linux__)
    void* library = ::dlopen(library_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (!library) {
        if (char* psz_error = ::dlerror()) { error_message = psz_error; }
    }
#endif

    if (!library) { app3d::logError("couldn't load dynamic library '{}': {}", library_path.filename(), error_message); }

    return library;
}

void app3d::freeDynamicLibrary(void* library) {
#if defined(WIN32)
    ::FreeLibrary(static_cast<HMODULE>(library));
#elif defined(__linux__)
    ::dlclose(library);
#endif
}

void* app3d::getDynamicLibraryEntry(void* library, const char* entry_name) {
#if defined(WIN32)
    void* entry = ::GetProcAddress(static_cast<HMODULE>(library), entry_name);
#elif defined(__linux__)
    void* entry = ::dlsym(library, entry_name);
#endif

    if (!entry) { app3d::logError("couldn't find entry point '{}'", entry_name); }

    return entry;
}
