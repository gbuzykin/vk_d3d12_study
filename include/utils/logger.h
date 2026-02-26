#pragma once

#include <uxs/format.h>     // NOLINT
#include <uxs/format_fs.h>  // NOLINT

#include <string_view>

namespace app3d {

enum class LogLevel : unsigned { PR_ERROR = 0, PR_WARNING, PR_INFO, PR_DEBUG };

void setPrintLevel(LogLevel level);

namespace detail {
void vprint(LogLevel level, std::string_view fmt, uxs::format_args args);
}

template<typename... Args>
void logError(uxs::format_string<Args...> fmt, const Args&... args) {
    detail::vprint(LogLevel::PR_ERROR, fmt.get(), uxs::make_format_args(args...));
}
template<typename... Args>
void logWarning(uxs::format_string<Args...> fmt, const Args&... args) {
    detail::vprint(LogLevel::PR_WARNING, fmt.get(), uxs::make_format_args(args...));
}
template<typename... Args>
void logInfo(uxs::format_string<Args...> fmt, const Args&... args) {
    detail::vprint(LogLevel::PR_INFO, fmt.get(), uxs::make_format_args(args...));
}

#if !defined(NDEBUG)
template<unsigned Level = 0, typename... Args>
void logDebug(uxs::format_string<Args...> fmt, const Args&... args) {
    detail::vprint(LogLevel{unsigned(LogLevel::PR_DEBUG) + Level}, fmt.get(), uxs::make_format_args(args...));
}
#else
template<unsigned Level = 0, typename... Dummy>
void logDebug(Dummy&&...) {}
#endif

}  // namespace app3d
