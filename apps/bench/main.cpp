#include <exception>
#include <expected>
#include <string>

#include "BenchOptions.h"
#include "common/AppChecks.h"
#include "core/CommandLine.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "rhi/AgilitySdk.h"

using engine::core::LogError;
using engine::core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

// The benchmark: runs a scene preset along a scripted camera path with fixed settings and writes per-frame
// times to CSV (plan section 3.7, rules in .claude/rules/bench-and-tools.md). Unattended: no dialogs.
int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    try {
        const std::expected<BenchOptions, std::string> options = ParseBenchOptions(engine::core::CommandLineArgs());
        if (!options) {
            LogError("{}", options.error());
            LogInfo("{}", kBenchUsage);
            return engine::apps::kExitBadCommandLine;
        }
        LogInfo("Bench: scene {}, grid {}, {} cubes, {}, {} warm-up + {} measured frames, results in '{}'",
                ToString(options->scene), engine::apps::ToString(options->gridMode), options->count,
                options->drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced",
                options->warmupFrames, options->frames, engine::core::ToUtf8(options->outDir.native()));
        // The run itself comes next (window, renderer, camera path, timings)
        return 0;
    } catch (const std::exception &e) {
        LogError("{}", e.what());
        return -1;
    }
}
