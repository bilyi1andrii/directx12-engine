#ifndef D3D12_ENGINE_BENCHOPTIONS_H
#define D3D12_ENGINE_BENCHOPTIONS_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "common/GridSetup.h"
#include "renderdata/RendererSettings.h"

// Command-line options of the bench app, with their defaults
struct BenchOptions {
    std::optional<std::filesystem::path> modelPath;                          // --model FILE; a cube without it
    apps::GridMode gridMode = apps::GridMode::Dynamic;                       // --grid static|dynamic
    uint32_t count = 50'000;                                                 // --count N; required with --model
    render::DrawPath drawPath = render::DrawPath::Instanced;                 // --instanced 1|0
    uint32_t frames = 1000;                                                  // --frames N, measured
    uint32_t warmupFrames = 200;                                             // --warmup N, not measured
    std::filesystem::path outDir = "results";                                // --out DIR
};

inline constexpr std::string_view kBenchUsage =
    "Usage: bench.exe [--model FILE] [--grid static|dynamic] [--count N] [--instanced 1|0]\n"
    "                 [--frames N] [--warmup N] [--out DIR]\n"
    "  --model FILE       the model to draw copies of; needs --count (default: a cube)\n"
    "  --grid MODE        static: uploaded once; dynamic: rebuilt and uploaded every frame (default)\n"
    "  --count N          copies in the grid, rounded up to a square n x n (default for the cube: 50000)\n"
    "  --instanced 1|0    1: one draw per bucket (default); 0: one draw per object\n"
    "  --frames N         measured frames, N >= 1 (default 1000)\n"
    "  --warmup N         frames rendered before measuring (default 200)\n"
    "  --out DIR          results folder (default results)";

// `args` without the program name. On failure the error says which argument is wrong.
[[nodiscard]] std::expected<BenchOptions, std::string> ParseBenchOptions(std::span<const std::string> args);

// What this run is called in its results folder and run.txt: "cube", or the model's <folder>-<stem>, because model
// files are often all called scene.gltf
[[nodiscard]] std::filesystem::path BenchSceneName(const BenchOptions &options);

#endif // D3D12_ENGINE_BENCHOPTIONS_H
