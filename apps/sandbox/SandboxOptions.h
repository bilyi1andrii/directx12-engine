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
};

inline constexpr std::string_view kSandboxUsage =
    "Usage: sandbox.exe [--smoke-test N] [--texture PATH]   (N: frames to render, >= 1; PATH: image for the grid)";

// args: the arguments without the program name. On failure the error says which argument is wrong.
[[nodiscard]] std::expected<SandboxOptions, std::string> ParseSandboxOptions(std::span<const std::string> args);

#endif // D3D12_ENGINE_SANDBOXOPTIONS_H
