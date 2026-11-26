#ifndef D3D12_ENGINE_APPCHECKS_H
#define D3D12_ENGINE_APPCHECKS_H

// End-of-run checks the sandbox and the bench share: the exit codes, the validation summary and the smoke test's
// verdict.

#include <cstdint>

#include "rhi/ValidationStats.h"

namespace apps {
    inline constexpr int kExitSmokeTestFailed = 1;
    inline constexpr int kExitBadCommandLine = 2;

    struct RunResult {
        int exitCode = 0;
        uint64_t framesRendered = 0;
    };

    void RouteCrtAssertsToStderr() noexcept;

    void LogValidationSummary(const rhi::ValidationStats &validation);

    [[nodiscard]] int SmokeTestExitCode(uint64_t expectedFrames, const RunResult &run,
                                        const rhi::ValidationStats &validation);
} // namespace apps

#endif // D3D12_ENGINE_APPCHECKS_H
