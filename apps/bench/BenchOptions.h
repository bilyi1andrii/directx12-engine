#ifndef D3D12_ENGINE_BENCHOPTIONS_H
#define D3D12_ENGINE_BENCHOPTIONS_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "common/StressScene.h"
#include "renderdata/RendererSettings.h"

// Sponza and the material grid come later
enum class BenchScene : uint8_t { Stress };

[[nodiscard]] std::string_view ToString(BenchScene scene) noexcept;

// Command-line options of the bench app, with their defaults
struct BenchOptions {
    BenchScene scene = BenchScene::Stress;                                   // --scene stress
    engine::apps::GridMode gridMode = engine::apps::GridMode::Dynamic;       // --grid static|dynamic
    uint32_t count = 50'000;                                                 // --count N
    engine::render::DrawPath drawPath = engine::render::DrawPath::Instanced; // --instanced 1|0
    uint32_t frames = 1000;                                                  // --frames N, measured
    uint32_t warmupFrames = 200;                                             // --warmup N, not measured
    std::filesystem::path outDir = "results";                                // --out DIR
};

inline constexpr std::string_view kBenchUsage =
    "Usage: bench.exe [--scene stress] [--grid static|dynamic] [--count CUBES] [--instanced 1|0]\n"
    "                 [--frames N] [--warmup N] [--out DIR]\n"
    "  --scene stress     the scene preset (default stress, the only one so far)\n"
    "  --grid MODE        static: uploaded once; dynamic: rebuilt and uploaded every frame (default)\n"
    "  --count CUBES      cubes in the stress grid, rounded up to a square n x n (default 50000)\n"
    "  --instanced 1|0    1: one draw per bucket (default); 0: one draw per object\n"
    "  --frames N         measured frames, N >= 1 (default 1000)\n"
    "  --warmup N         frames rendered before measuring (default 200)\n"
    "  --out DIR          results folder (default results)";

// `args` without the program name. On failure the error says which argument is wrong.
[[nodiscard]] std::expected<BenchOptions, std::string> ParseBenchOptions(std::span<const std::string> args);

#endif // D3D12_ENGINE_BENCHOPTIONS_H
