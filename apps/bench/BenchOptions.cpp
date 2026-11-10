#include "BenchOptions.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <system_error>
#include <vector>

#include "common/OptionValues.h"
#include "core/Utf8.h"

using engine::apps::ParseUint32;

namespace {
    // Each of them takes one value
    constexpr std::array<std::string_view, 7> kOptionNames{"--model",  "--grid",   "--count", "--instanced",
                                                           "--frames", "--warmup", "--out"};
} // namespace

std::expected<BenchOptions, std::string> ParseBenchOptions(std::span<const std::string> args) {
    BenchOptions options;
    std::vector<std::string_view> seen; // to reject an option given twice
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (std::ranges::find(kOptionNames, arg) == kOptionNames.end()) {
            return std::unexpected(std::format("Unknown argument '{}'", arg));
        }
        if (std::ranges::find(seen, arg) != seen.end()) {
            return std::unexpected(std::format("{} is given more than once", arg));
        }
        seen.push_back(arg);
        if (i + 1 == args.size()) {
            return std::unexpected(std::format("{} needs a value", arg));
        }
        const std::string_view value = args[++i];

        if (arg == "--model") {
            if (value.empty()) {
                return std::unexpected("--model needs a file");
            }
            options.modelPath = engine::core::PathFromUtf8(value);
        } else if (arg == "--grid") {
            const std::optional<engine::apps::GridMode> mode = engine::apps::ParseGridMode(value);
            if (!mode) {
                return std::unexpected(std::format("--grid needs static or dynamic, got '{}'", value));
            }
            options.gridMode = *mode;
        } else if (arg == "--count") {
            const std::optional<uint32_t> count = ParseUint32(value);
            if (!count) {
                return std::unexpected(std::format("--count needs a number of copies >= 0, got '{}'", value));
            }
            options.count = *count;
        } else if (arg == "--instanced") {
            if (value != "0" && value != "1") {
                return std::unexpected(std::format("--instanced needs 1 or 0, got '{}'", value));
            }
            options.drawPath =
                value == "1" ? engine::render::DrawPath::Instanced : engine::render::DrawPath::NonInstanced;
        } else if (arg == "--frames") {
            const std::optional<uint32_t> frames = ParseUint32(value);
            if (!frames || *frames == 0) {
                return std::unexpected(std::format("--frames needs a frame count >= 1, got '{}'", value));
            }
            options.frames = *frames;
        } else if (arg == "--warmup") {
            const std::optional<uint32_t> frames = ParseUint32(value);
            if (!frames) {
                return std::unexpected(std::format("--warmup needs a frame count >= 0, got '{}'", value));
            }
            options.warmupFrames = *frames;
        } else { // --out
            if (value.empty()) {
                return std::unexpected("--out needs a folder");
            }
            options.outDir = engine::core::PathFromUtf8(value);
        }
    }
    // The default suits a cube: 50,000 copies of a detailed model are billions of triangles per frame
    if (options.modelPath && std::ranges::find(seen, std::string_view{"--count"}) == seen.end()) {
        return std::unexpected("--model needs --count, the number of copies");
    }
    return options;
}

std::filesystem::path BenchSceneName(const BenchOptions &options) {
    if (!options.modelPath) {
        return L"cube";
    }
    // Absolute, because a bare file name has no folder in it
    std::error_code error;
    std::filesystem::path full = std::filesystem::absolute(*options.modelPath, error);
    if (error) {
        full = *options.modelPath;
    }
    full = full.lexically_normal();
    const std::wstring folder = full.parent_path().filename().wstring();
    return folder.empty() ? full.stem().wstring() : folder + L"-" + full.stem().wstring();
}
