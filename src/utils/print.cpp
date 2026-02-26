#include "utils/print.h"

#include <uxs/format_chrono.h>
#include <uxs/io/iobuf.h>

#include <ctime>
#include <mutex>

namespace {
uxs::iobuf& g_out = uxs::stdbuf::out();
uxs::iobuf& g_err = uxs::stdbuf::err();
std::mutex g_mtx;
app3d::LogLevel g_current_print_level{app3d::LogLevel::PR_INFO};

std::chrono::seconds currentTimeZoneOffset() {
    using namespace std::chrono;
    using namespace std::chrono_literals;
    static std::once_flag fl;
    static seconds zone_offset;
    std::call_once(fl, []() {
#ifdef __GNUC__
        std::tm tm{};
        tm.tm_year = 100;
        tm.tm_mday = 1;
        std::time_t t = std::mktime(&tm);
        std::tm gmtm = *std::gmtime(&t);

        zone_offset = sys_days{year_month_day{2000y, January, 1d}} -
                      sys_days{year_month_day{year{1900 + gmtm.tm_year}, month{1U + static_cast<unsigned>(gmtm.tm_mon)},
                                              day{static_cast<unsigned>(gmtm.tm_mday)}}} -
                      hours(gmtm.tm_hour) - minutes(gmtm.tm_min) - seconds(gmtm.tm_sec);
#else
        const auto info = current_zone()->get_info(system_clock::now());
        zone_offset = info.offset;
#endif
    });
    return zone_offset;
}
}  // namespace

void app3d::setPrintLevel(LogLevel level) { g_current_print_level = level; }

void app3d::detail::vprint(LogLevel level, std::string_view fmt, uxs::format_args args) {
    if (level > g_current_print_level) { return; }

    std::string_view msg_type;
    switch (level) {
        case LogLevel::PR_ERROR: msg_type = "\033[0;31merror: "; break;
        case LogLevel::PR_WARNING: msg_type = "\033[0;35mwarning: "; break;
        case LogLevel::PR_DEBUG: {
#if !defined(NDEBUG)
            msg_type = "\033[0;33mdebug: \033[0m";
#else
            return;
#endif
        } break;
        default: break;
    }

    using namespace std::chrono;
    auto now = floor<seconds>(system_clock::now() + currentTimeZoneOffset());

    auto& obuf = level <= LogLevel::PR_WARNING ? g_err : g_out;

    using namespace std::literals;

    std::lock_guard lk(g_mtx);
    uxs::print(obuf, "{}> {}", now, msg_type);
    uxs::vprint(obuf, fmt, args).endl();
    obuf.write("\033[0m"sv);
}
