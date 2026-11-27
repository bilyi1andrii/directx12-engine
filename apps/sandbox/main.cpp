// The sandbox: a window with the cube grid, models and lights to look around, select and move, and the debug UI. It
// saves the scene when it closes.

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "SandboxApp.h"
#include "SandboxOptions.h"
#include "common/AppChecks.h"
#include "common/AppMain.h"
#include "common/AppShell.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"

using core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    apps::RunResult RunSandbox(const SandboxOptions &options, rhi::ValidationStats &validation) {
        const core::Timer initTimer;
        apps::AppShell shell({.title = L"D3D12 Engine", .ui = true}, validation);
        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), shell.Window().Width(),
                shell.Window().Height(), shell.Renderer().TearingSupported() ? "supported" : "not supported");

        sandbox::SandboxApp app(shell, options);
        return app.Run();
    }
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    std::optional<SandboxOptions> options;
    apps::RunResult run;
    return apps::AppMain({
        .usage = kSandboxUsage,
        .errorDialog = true,
        .parse = [&options](std::span<const std::string> args) -> std::expected<apps::RunKind, std::string> {
            std::expected<SandboxOptions, std::string> parsed = ParseSandboxOptions(args);
            if (!parsed) {
                return std::unexpected(std::move(parsed.error()));
            }
            options = std::move(*parsed);
            return options->smokeTestFrames ? apps::RunKind::Unattended : apps::RunKind::Interactive;
        },
        .run = [&](rhi::ValidationStats &validation) { run = RunSandbox(*options, validation); },
        .finish =
            [&](const rhi::ValidationStats &validation) {
                const int exitCode = options->smokeTestFrames
                                         ? apps::SmokeTestExitCode(*options->smokeTestFrames, run, validation)
                                         : run.exitCode;
                LogInfo("Exiting with code {}", exitCode);
                return exitCode;
            },
    });
}
