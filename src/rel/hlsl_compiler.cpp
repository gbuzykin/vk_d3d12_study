#include "rel/hlsl_compiler.h"

#include "common/dynamic_library.h"
#include "common/logger.h"
#include "util/ref_ptr.h"

#include <uxs/db/json.h>
#include <uxs/dynarray.h>
#include <uxs/string_util.h>

#include <mutex>

// clang-format off
#ifdef _WIN32
#    include <combaseapi.h>
#endif  // _WIN32
#include <dxc/dxcapi.h>
// clang-format on

using namespace app3d;
using namespace app3d::rel;

// --------------------------------------------------------
// HlslCompiler::Implementation class implementation

class HlslCompiler::Implementation {
 public:
    ~Implementation();

    DataBlob compileShader(const DataBlob& source_text, const uxs::db::basic_value<wchar_t>& args,
                           DataBlob& compiler_output);

    void setPlatformArgs(uxs::db::basic_value<wchar_t> platform_args) { platform_args_ = std::move(platform_args); }

 private:
    static std::atomic<bool> is_initialized_;
    std::mutex mtx_;
    void* dxcompiler_library_ = nullptr;
    DxcCreateInstanceProc create_proc_ = nullptr;
    util::ref_ptr<IDxcUtils> dxc_utils_;
    util::ref_ptr<IDxcCompiler3> dxc_compiler_;
    util::ref_ptr<IDxcIncludeHandler> include_handler_;
    uxs::db::basic_value<wchar_t> platform_args_;

    bool init();
};

std::atomic<bool> HlslCompiler::Implementation::is_initialized_{};

HlslCompiler::Implementation::~Implementation() {
    include_handler_.reset();
    dxc_compiler_.reset();
    dxc_utils_.reset();
    if (dxcompiler_library_) { freeDynamicLibrary(dxcompiler_library_); }
}

bool HlslCompiler::Implementation::init() {
    std::lock_guard lk(mtx_);

    if (!dxcompiler_library_) {
        dxcompiler_library_ = loadDynamicLibrary("", "dxcompiler");
        if (!dxcompiler_library_) { return false; }
    }

    create_proc_ = (DxcCreateInstanceProc)getDynamicLibraryEntry(dxcompiler_library_, "DxcCreateInstance");
    if (!create_proc_) {
        logError("couldn't load DxcCreateInstance function");
        return false;
    }

    HRESULT result = create_proc_(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utils_.reset_and_get_address()));
    if (result != S_OK) {
        logError("couldn't create DxcUtils object");
        return false;
    }

    result = create_proc_(CLSID_DxcCompiler, IID_PPV_ARGS(dxc_compiler_.reset_and_get_address()));
    if (result != S_OK) {
        logError("couldn't create DxcCompiler3 object");
        return false;
    }

    result = dxc_utils_->CreateDefaultIncludeHandler(include_handler_.reset_and_get_address());
    if (result != S_OK) {
        logError("couldn't create DxcIncludeHandler object");
        return false;
    }

    return true;
}

DataBlob HlslCompiler::Implementation::compileShader(const DataBlob& source_text,
                                                     const uxs::db::basic_value<wchar_t>& args,
                                                     DataBlob& compiler_output) {
    if (!is_initialized_) {
        if (!init()) { return {}; }
    }

    uxs::inline_dynarray<LPCWSTR, 64> args_array;

    const wchar_t* filename = args.value_or<const wchar_t*>(L"filename", L"");

    args_array.push_back(filename);
    args_array.push_back(L"-E");
    args_array.push_back(args.value_or<const wchar_t*>(L"entry", L"main"));

    if (const wchar_t* target = args.value<const wchar_t*>(L"target")) {
        args_array.push_back(L"-T");
        args_array.push_back(target);
    }

    for (const auto& arg_list = args.value(L"args"); const auto& arg : arg_list.as_array()) {
        args_array.push_back(arg.as_c_string());
    }

    for (const auto& arg_list = platform_args_.value(L"args"); const auto& arg : arg_list.as_array()) {
        args_array.push_back(arg.as_c_string());
    }

    const DxcBuffer source{
        .Ptr = source_text.getData(),
        .Size = SIZE_T(source_text.getSize()),
        .Encoding = DXC_CP_ACP,
    };

    std::unique_lock lk(mtx_);

    util::ref_ptr<IDxcResult> results;
    dxc_compiler_->Compile(&source, args_array.data(), UINT32(args_array.size()), &*include_handler_,
                           IID_PPV_ARGS(results.reset_and_get_address()));

    lk.unlock();

    util::ref_ptr<IDxcBlobUtf8> errors;
    results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.reset_and_get_address()), nullptr);
    const std::size_t errors_length = errors ? errors->GetStringLength() : 0;
    if (errors_length) {
        compiler_output = DataBlob(errors_length);
        std::memcpy(compiler_output.getData(), errors->GetStringPointer(), errors_length);
    } else {
        compiler_output = DataBlob();
    }

    HRESULT status = S_OK;
    results->GetStatus(&status);
    if (FAILED(status)) {
        logError("shader '{}' compilation error: {:#010x}", uxs::utf8_string_adapter{}(filename), status);
        return {};
    }

    util::ref_ptr<IDxcBlob> shader;
    results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader.reset_and_get_address()), nullptr);
    const std::size_t binary_length = shader ? shader->GetBufferSize() : 0;
    if (binary_length) {
        DataBlob data_blob(binary_length);
        std::memcpy(data_blob.getData(), shader->GetBufferPointer(), binary_length);
        return data_blob;
    }

    return {};
}

// --------------------------------------------------------
// HlslCompiler class implementation

HlslCompiler::HlslCompiler() : impl_(std::make_unique<Implementation>()) {}

HlslCompiler::~HlslCompiler() {}

namespace {
uxs::db::basic_value<wchar_t> uft8ToWideUtfDbValue(const uxs::db::value& v) {
    return v.visit([](auto x) -> uxs::db::basic_value<wchar_t> {
        uxs::db::basic_value<wchar_t> result;
        if constexpr (std::is_same_v<decltype(x), decltype(v.as_string_view())>) {
            return std::wstring_view{uxs::wide_string_adapter{}(x)};
        } else if constexpr (std::is_same_v<decltype(x), decltype(v.as_array())>) {
            result.reserve(x.size());
            for (const auto& el : x) { result.emplace_back(uft8ToWideUtfDbValue(el)); }
        } else if constexpr (std::is_same_v<decltype(x), decltype(v.as_record())>) {
            result.reserve(uxs::db::record_tag, x.size());
            for (const auto& [key, value] : x) {
                result.emplace(uxs::wide_string_adapter{}(key), uft8ToWideUtfDbValue(value));
            }
        } else if constexpr (!std::is_same_v<decltype(x), std::nullptr_t>) {
            return x;
        }
        return result;
    });
}
}  // namespace

DataBlob HlslCompiler::compileShader(const DataBlob& source_text, const uxs::db::value& args,
                                     DataBlob& compiler_output) {
    return impl_->compileShader(source_text, uft8ToWideUtfDbValue(args), compiler_output);
}

void HlslCompiler::setPlatformArgs(const uxs::db::value& platform_args) {
    impl_->setPlatformArgs(uft8ToWideUtfDbValue(platform_args));
}
