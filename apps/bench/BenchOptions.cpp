#include "BenchOptions.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <vector>

#include "common/OptionValues.h"
#include "core/Utf8.h"

namespace fs = std::filesystem;

using apps::ParseUint32;

namespace {
    constexpr std::array<std::string_view, 7> kOptionNames{"--model",  "--grid",   "--count", "--instanced",
                                                           "--frames", "--warmup", "--out"};
} // namespace

std::expected<BenchOptions, std::string> ParseBenchOptions(std::span<const std::string> args) {
    BenchOptions options;
    std::vector<std::string_view> seen;
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
            options.modelPath = core::PathFromUtf8(value);
        } else if (arg == "--grid") {
            const std::optional<apps::GridMode> mode = apps::ParseGridMode(value);
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
            options.drawPath = value == "1" ? render::DrawPath::Instanced : render::DrawPath::NonInstanced;
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
        } else {
            if (value.empty()) {
                return std::unexpected("--out needs a folder");
            }
            options.outDir = core::PathFromUtf8(value);
        }
    }
    if (options.modelPath && std::ranges::find(seen, std::string_view{"--count"}) == seen.end()) {
        return std::unexpected("--model needs --count, the number of copies");
    }
    return options;
}

fs::path BenchSceneName(const BenchOptions &options) {
    if (!options.modelPath) {
        return L"cube";
    }
    const fs::path full = fs::absolute(*options.modelPath).lexically_normal();
    const std::wstring folder = full.parent_path().filename().wstring();
    return folder.empty() ? full.stem().wstring() : folder + L"-" + full.stem().wstring();
}
