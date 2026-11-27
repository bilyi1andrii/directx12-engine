#ifndef D3D12_ENGINE_APPMAIN_H
#define D3D12_ENGINE_APPMAIN_H

// The start and end of every app: reads the command line, runs the app, logs the validation summary and turns the
// outcome into the process exit code. An error that escapes the app is logged, with a dialog only for a person.

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>

#include "rhi/ValidationStats.h"

namespace apps {
    enum class RunKind : uint8_t { Interactive, Unattended };

    struct AppMainDesc {
        std::string_view usage;
        bool errorDialog = false;
        std::function<std::expected<RunKind, std::string>(std::span<const std::string> args)> parse;
        std::function<void(rhi::ValidationStats &validation)> run;
        std::function<int(const rhi::ValidationStats &validation)> finish;
    };

    [[nodiscard]] int AppMain(const AppMainDesc &desc);
} // namespace apps

#endif // D3D12_ENGINE_APPMAIN_H
