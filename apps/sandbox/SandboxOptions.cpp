#include "SandboxOptions.h"

#include <format>

#include "common/OptionValues.h"

using engine::apps::ParseUint32;

std::expected<SandboxOptions, std::string> ParseSandboxOptions(std::span<const std::string> args) {
    SandboxOptions options;
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        if (arg == "--smoke-test") {
            if (options.smokeTestFrames) {
                return std::unexpected("--smoke-test is given more than once");
            }
            if (i + 1 == args.size()) {
                return std::unexpected("--smoke-test needs a frame count");
            }
            const std::string_view value = args[++i];
            const std::optional<uint32_t> frames = ParseUint32(value);
            if (!frames || *frames == 0) {
                return std::unexpected(std::format("--smoke-test needs a frame count >= 1, got '{}'", value));
            }
            options.smokeTestFrames = frames;
        } else if (arg == "--count") {
            if (options.gridCount) {
                return std::unexpected("--count is given more than once");
            }
            if (i + 1 == args.size()) {
                return std::unexpected("--count needs a number of cubes");
            }
            const std::string_view value = args[++i];
            const std::optional<uint32_t> count = ParseUint32(value);
            if (!count) {
                return std::unexpected(std::format("--count needs a number of cubes >= 0, got '{}'", value));
            }
            options.gridCount = count;
        } else if (arg == "--grid") {
            if (options.gridMode) {
                return std::unexpected("--grid is given more than once");
            }
            if (i + 1 == args.size()) {
                return std::unexpected("--grid needs static or dynamic");
            }
            const std::string_view value = args[++i];
            const std::optional<engine::apps::GridMode> mode = engine::apps::ParseGridMode(value);
            if (!mode) {
                return std::unexpected(std::format("--grid needs static or dynamic, got '{}'", value));
            }
            options.gridMode = mode;
        } else if (arg == "--texture") {
            if (options.texturePath) {
                return std::unexpected("--texture is given more than once");
            }
            if (i + 1 == args.size() || args[i + 1].empty()) {
                return std::unexpected("--texture needs a file path");
            }
            options.texturePath = engine::apps::PathFromUtf8(args[++i]);
        } else if (arg == "--model") {
            if (options.modelPath) {
                return std::unexpected("--model is given more than once");
            }
            if (i + 1 == args.size() || args[i + 1].empty()) {
                return std::unexpected("--model needs a file path");
            }
            options.modelPath = engine::apps::PathFromUtf8(args[++i]);
        } else {
            return std::unexpected(std::format("Unknown argument '{}'", arg));
        }
    }
    return options;
}
