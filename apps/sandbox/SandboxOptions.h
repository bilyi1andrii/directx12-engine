#ifndef D3D12_ENGINE_SANDBOXOPTIONS_H
#define D3D12_ENGINE_SANDBOXOPTIONS_H

// The sandbox's command-line options.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "common/GridSetup.h"

struct SandboxOptions {
    std::optional<uint32_t> smokeTestFrames;
    std::optional<std::filesystem::path> texturePath;
    std::optional<uint32_t> gridCount;
    std::optional<apps::GridMode> gridMode;
    std::optional<std::filesystem::path> modelPath;
};

inline constexpr std::string_view kSandboxUsage =
    "Usage: sandbox.exe [--smoke-test FRAMES] [--count CUBES] [--grid static|dynamic] [--texture PATH] [--model PATH]\n"
    "  --smoke-test FRAMES  render FRAMES (>= 1) frames, then exit with the test result\n"
    "  --count CUBES        cubes in the grid, rounded up to a square n x n (default 400 = 20 x 20)\n"
    "  --grid MODE          static: uploaded once; dynamic: rebuilt and uploaded every frame (default)\n"
    "  --texture PATH       image file shown on the grid instead of data/textures/Endstone.jpg\n"
    "  --model PATH         model file shown on the grid (default: every model in assets/)";

[[nodiscard]] std::expected<SandboxOptions, std::string> ParseSandboxOptions(std::span<const std::string> args);

#endif // D3D12_ENGINE_SANDBOXOPTIONS_H
