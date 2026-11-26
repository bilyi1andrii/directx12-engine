#include "AppChecks.h"

#include <cstdlib>
#include <format>
#include <string>
#include <vector>

#include "core/Log.h"

namespace apps {
    namespace {
#if defined(_DEBUG)
        constexpr bool kValidationExpected = true;
#else
        constexpr bool kValidationExpected = false;
#endif
    } // namespace

    void RouteCrtAssertsToStderr() noexcept {
        // wWinMain makes assert() show a message box, even in a console build: send it to stderr instead
        _set_error_mode(_OUT_TO_STDERR);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    }

    void LogValidationSummary(const rhi::ValidationStats &validation) {
        if (!validation.active) {
            core::LogInfo("Validation: not active");
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
} // namespace apps
