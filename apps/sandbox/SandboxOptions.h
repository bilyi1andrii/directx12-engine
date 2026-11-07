#ifndef D3D12_ENGINE_SANDBOXOPTIONS_H
#define D3D12_ENGINE_SANDBOXOPTIONS_H

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

// Command-line options of the sandbox app
struct SandboxOptions {
    // --smoke-test N: render N frames, then exit; the exit code reports debug-layer errors and leaks
    std::optional<uint32_t> smokeTestFrames;
    // --texture PATH: image for the cube grid instead of the checkerboard (.png, .jpg, .dds, ...)
    std::optional<std::filesystem::path> texturePath;
    // --count N: cubes in the grid (0 or more), rounded up to a square n x n; without it, kDefaultGridCount
    std::optional<uint32_t> gridCount;
};

// The grid without --count: 20 x 20
inline constexpr uint32_t kDefaultGridCount = 400;

inline constexpr std::string_view kSandboxUsage =
    "Usage: sandbox.exe [--smoke-test FRAMES] [--count CUBES] [--texture PATH]\n"
    "  --smoke-test FRAMES  render FRAMES (>= 1) frames, then exit with the test result\n"
    "  --count CUBES        cubes in the grid, rounded up to a square n x n (default 400 = 20 x 20)\n"
    "  --texture PATH       image file shown on the grid instead of the checkerboard";

// args: the arguments without the program name. On failure the error says which argument is wrong.
[[nodiscard]] std::expected<SandboxOptions, std::string> ParseSandboxOptions(std::span<const std::string> args);

#endif // D3D12_ENGINE_SANDBOXOPTIONS_H
