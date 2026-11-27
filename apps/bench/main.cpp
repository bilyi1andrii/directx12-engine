// The benchmark: flies a scripted camera over a grid of copies of a model and writes the time of every frame to
// results/dx12/<scene>/<timestamp>/. It runs unattended, so it never shows a dialog.

#include <chrono>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "BenchApp.h"
#include "BenchOptions.h"
#include "BenchReport.h"
#include "assets/AssetCache.h"
#include "common/AppMain.h"
#include "common/AppShell.h"
#include "common/GridSetup.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "renderdata/Model.h"

namespace chrono = std::chrono;

using core::LogInfo;
using core::LogWarning;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    std::expected<const render::Model *, std::string> LoadGridModel(const BenchOptions &options,
                                                                    assets::AssetCache &assets) {
        if (!options.modelPath) {
            return &assets.CubeModel(assets.CheckerboardTexture());
        }
        const auto model = assets.LoadModel(*options.modelPath);
        if (!model) {
            return std::unexpected(model.error().message);
        }
        return *model;
    }

    std::expected<bench::BenchRun, std::string> RunBench(const BenchOptions &options,
                                                         rhi::ValidationStats &validation) {
        apps::AppShell shell({.title = L"D3D12 Engine - bench", .fullscreen = true}, validation);
        if (!shell.Renderer().TearingSupported()) {
            LogWarning("Tearing not supported: Present without vsync may still wait for the display");
        }

        assets::AssetCache assets(shell.Renderer().Resources());
        const std::expected<const render::Model *, std::string> model = LoadGridModel(options, assets);
        if (!model) {
            return std::unexpected(model.error());
        }

        bench::BenchApp app(shell, options, **model);
        return app.Run();
    }
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    std::optional<BenchOptions> options;
    chrono::system_clock::time_point started;
    std::optional<std::expected<bench::BenchRun, std::string>> outcome;
    return apps::AppMain({
        .usage = kBenchUsage,
        .errorDialog = false,
        .parse = [&options](std::span<const std::string> args) -> std::expected<apps::RunKind, std::string> {
            std::expected<BenchOptions, std::string> parsed = ParseBenchOptions(args);
            if (!parsed) {
                return std::unexpected(std::move(parsed.error()));
            }
            options = std::move(*parsed);
            return apps::RunKind::Unattended;
        },
        .run =
            [&](rhi::ValidationStats &validation) {
                LogInfo("Bench: {}, grid {}, --count {}, {}, results in '{}'",
                        core::ToUtf8(BenchSceneName(*options).native()), apps::ToString(options->gridMode),
                        options->count,
                        options->drawPath == render::DrawPath::Instanced ? "instanced" : "non-instanced",
                        core::ToUtf8(options->outDir.native()));
                started = chrono::system_clock::now();
                outcome = RunBench(*options, validation);
            },
        .finish =
            [&](const rhi::ValidationStats &validation) {
                return bench::ReportRun(*options, *outcome, started, validation);
            },
    });
}
