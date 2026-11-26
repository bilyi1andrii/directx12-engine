#ifndef D3D12_ENGINE_BENCHOPTIONS_H
#define D3D12_ENGINE_BENCHOPTIONS_H

// The bench's command-line options, and the name its results folder gets from them.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "common/GridSetup.h"
#include "renderdata/RendererSettings.h"

struct BenchOptions {
    std::optional<std::filesystem::path> modelPath;
    apps::GridMode gridMode = apps::GridMode::Dynamic;
    uint32_t count = 50'000;
    render::DrawPath drawPath = render::DrawPath::Instanced;
    uint32_t frames = 1000;
    uint32_t warmupFrames = 200;
    std::filesystem::path outDir = "results";
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

[[nodiscard]] std::expected<BenchOptions, std::string> ParseBenchOptions(std::span<const std::string> args);

[[nodiscard]] std::filesystem::path BenchSceneName(const BenchOptions &options);

#endif // D3D12_ENGINE_BENCHOPTIONS_H
