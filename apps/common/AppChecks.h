#ifndef D3D12_ENGINE_APPCHECKS_H
#define D3D12_ENGINE_APPCHECKS_H

#include <cstdint>

#include "rhi/ValidationStats.h"

// End-of-run checks shared by the apps (sandbox, bench)
namespace engine::apps {
    // Process exit codes besides the window's own (0). An assert exits with 3, an exception with -1.
    inline constexpr int kExitSmokeTestFailed = 1;
    inline constexpr int kExitBadCommandLine = 2;

    // How a run ended
    struct RunResult {
        int exitCode = 0; // the window's
        uint64_t framesRendered = 0;
    };

    // For unattended runs (smoke test, bench), call once at startup. Third-party asserts (ImGui's IM_ASSERT,
    // D3D12MA_ASSERT) are the CRT's assert(), which in our apps shows a message box and waits for a click, so ctest
    // would only end at its timeout. Afterwards a failed assert() prints to stderr and exits with code 3, like
    // ENGINE_ASSERT. (Only Debug builds have them: Release libraries are built with NDEBUG.)
    void RouteCrtAssertsToStderr() noexcept;

    // Logs the debug-layer errors and leaks of a finished run (or that validation was not active)
    void LogValidationSummary(const rhi::ValidationStats &validation);

    // Checks an unattended run and logs the result: all frames rendered, window exit code 0, no debug-layer
    // errors, no leaks, and (Debug) validation active. Returns the process exit code.
    [[nodiscard]] int SmokeTestExitCode(uint64_t expectedFrames, const RunResult &run,
                                        const rhi::ValidationStats &validation);
} // namespace engine::apps

#endif // D3D12_ENGINE_APPCHECKS_H
