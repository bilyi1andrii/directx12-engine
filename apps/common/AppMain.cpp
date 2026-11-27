#include "AppMain.h"

#include <exception>
#include <vector>

#include <Windows.h>

#include "AppChecks.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/Log.h"

namespace apps {
    int AppMain(const AppMainDesc &desc) {
        bool errorDialog = desc.errorDialog;
        try {
            const std::vector<std::string> args = core::CommandLineArgs();
            const std::expected<RunKind, std::string> kind = desc.parse(args);
            if (!kind) {
                core::LogError("{}", kind.error());
                core::LogInfo("{}", desc.usage);
                return kExitBadCommandLine;
            }
            if (*kind == RunKind::Unattended) {
                // No dialog in a smoke test: it would block the unattended run
                errorDialog = false;
                RouteCrtAssertsToStderr();
            }

            const core::ComScope com;

            rhi::ValidationStats validation;
            desc.run(validation);
            LogValidationSummary(validation);
            return desc.finish(validation);
        } catch (const std::exception &e) {
            core::LogError("{}", e.what());
            if (errorDialog) {
                MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
            }
            return -1;
        }
    }
} // namespace apps
