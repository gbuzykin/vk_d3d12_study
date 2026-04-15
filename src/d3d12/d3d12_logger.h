#pragma once

#include "common/logger.h"

#include <d3d12.h>

#ifdef min
#    undef min
#endif

#ifdef max
#    undef max
#endif

#define LOG_D3D12 "D3D12: "

namespace app3d::rel::d3d12 {
struct D3D12Result {
    HRESULT result;
};
std::string_view getHResultText(HRESULT result);
}  // namespace app3d::rel::d3d12

template<typename CharT>
struct uxs::formatter<app3d::rel::d3d12::D3D12Result, CharT> {
 public:
    template<typename ParseCtx>
    constexpr typename ParseCtx::iterator parse(ParseCtx& ctx) {
        auto it = ctx.begin();
        return it == ctx.end() || *it != ':' ? it : it + 1;
    }
    template<typename FmtCtx>
    void format(FmtCtx& ctx, app3d::rel::d3d12::D3D12Result val) const {
        const auto text = app3d::rel::d3d12::getHResultText(val.result);
        if (!text.empty()) {
            ctx.out() += text;
        } else {
            to_basic_string(ctx.out(), std::uint32_t(val.result),
                            fmt_opts(fmt_flags::hex | fmt_flags::leading_zeroes | fmt_flags::alternate, -1, 10));
        }
    }
};
