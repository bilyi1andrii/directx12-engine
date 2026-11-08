#include "AppChecks.h"

#include <cstdlib>
#include <format>
#include <string>
#include <vector>

#include "core/Log.h"

namespace engine::apps {
    namespace {
        // The debug layer exists only in Debug builds (rhi::Device checks the same macro)
#if defined(_DEBUG)
        constexpr bool kValidationExpected = true;
#else
        constexpr bool kValidationExpected = false;
#endif
    } // namespace

    void RouteCrtAssertsToStderr() noexcept {
        // assert() picks a message box or stderr by the error mode. The default follows the app type, which the
        // wWinMain entry point sets to "GUI" even in our console-subsystem builds. (_CrtSetReportMode does not
        // apply: it configures _ASSERT/_ASSERTE, not assert().)
        _set_error_mode(_OUT_TO_STDERR);
        // assert() then calls abort(): no "abort() has been called" dialog, no Windows Error Reporting
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    }

    void LogValidationSummary(const rhi::ValidationStats &validation) {
        if (!validation.active) {
            if (kValidationExpected) {
                core::LogWarning("Validation: not active (D3D12 or DXGI debug interface unavailable)");
            } else {
                core::LogInfo("Validation: not active (no debug layer in this build)");
            }
            return;
        }
        const uint32_t errors = validation.errorCount.load();
        if (errors == 0 && validation.liveObjectCount == 0) {
            core::LogInfo("Validation: 0 debug-layer errors, 0 live objects at shutdown");
        } else {
            core::LogWarning("Validation: {} debug-layer errors, {} live objects at shutdown", errors,
                             validation.liveObjectCount);
        }
    }

    int SmokeTestExitCode(uint64_t expectedFrames, const RunResult &run, const rhi::ValidationStats &validation) {
        std::vector<std::string> failures;
        if (run.framesRendered != expectedFrames) {
            // e.g. the window was closed early (Alt+F4, WM_CLOSE from outside)
            failures.push_back(std::format("rendered {} of {} frames", run.framesRendered, expectedFrames));
        }
        if (run.exitCode != 0) {
            failures.push_back(std::format("window exit code {}", run.exitCode));
        }
        if (validation.active) {
            const uint32_t errors = validation.errorCount.load();
            if (errors != 0) {
                failures.push_back(std::format("{} debug-layer errors", errors));
            }
            if (validation.liveObjectCount != 0) {
                failures.push_back(std::format("{} live objects at shutdown", validation.liveObjectCount));
            }
        } else if (kValidationExpected) {
            // Fail closed: a Debug run that could not check anything must not pass
            failures.emplace_back("validation was not active");
        } else {
            core::LogWarning("Smoke test: no debug layer in this build; only checks that {} frames render and the "
                             "app shuts down",
                             expectedFrames);
        }

        if (failures.empty()) {
            core::LogInfo("Smoke test PASSED: {} frames", run.framesRendered);
            return 0;
        }
        std::string reasons;
        for (const std::string &failure : failures) {
            reasons += reasons.empty() ? failure : "; " + failure;
        }
        core::LogError("Smoke test FAILED: {}", reasons);
        return kExitSmokeTestFailed;
    }
} // namespace engine::apps
