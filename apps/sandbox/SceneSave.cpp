#include "sandbox/SceneSave.h"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/Utf8.h"

namespace fs = std::filesystem;

namespace sandbox {
    namespace {
        using core::Error;
        using core::ErrorCode;
        using nlohmann::json;

        [[nodiscard]] json MatrixToJson(const DirectX::XMFLOAT4X4 &matrix) {
            json values = json::array();
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    values.push_back(matrix.m[row][column]);
                }
            }
            return values;
        }

        [[nodiscard]] DirectX::XMFLOAT4X4 MatrixFromJson(const json &values) {
            if (!values.is_array() || values.size() != 16) {
                throw std::runtime_error("a matrix must be 16 numbers");
            }
            DirectX::XMFLOAT4X4 matrix{};
            for (int i = 0; i < 16; ++i) {
                matrix.m[i / 4][i % 4] = values.at(i).get<float>();
            }
            return matrix;
        }

        [[nodiscard]] std::string PathToJson(const fs::path &path, const fs::path &root) {
            const fs::path relative = path.lexically_normal().lexically_relative(root.lexically_normal());
            const bool inside = !relative.empty() && *relative.begin() != "..";
            return core::ToUtf8((inside ? relative : path).generic_wstring());
        }

        [[nodiscard]] fs::path PathFromJson(const std::string &text, const fs::path &root) {
            const fs::path path(std::u8string(text.begin(), text.end()));
            return (path.is_absolute() ? path : root / path).lexically_normal();
        }
    } // namespace

    std::expected<SandboxSave, Error> ReadSandboxSave(const fs::path &file, const fs::path &root) {
        const std::string name = core::ToUtf8(file.native());
        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) {
            return std::unexpected(Error{ErrorCode::FileNotFound, std::format("{}: file not found", name)});
        }
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return std::unexpected(Error{ErrorCode::ReadFailed, std::format("{}: could not open the file", name)});
        }
        const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};

        try {
            const json parsed = json::parse(text);
            const int version = parsed.at("version").get<int>();
            if (version != kSaveVersion) {
                return std::unexpected(Error{ErrorCode::UnsupportedFormat,
                                             std::format("{}: version {}, expected {}", name, version, kSaveVersion)});
            }
            SandboxSave save;
            if (const auto grid = parsed.find("grid"); grid != parsed.end()) {
                save.gridCount = grid->value("count", 0u);
                save.gridMode = grid->value("mode", std::string{});
                for (const json &cube : grid->value("cubes", json::array())) {
                    save.gridCubes.push_back(
                        {.index = cube.at("index").get<uint32_t>(), .world = MatrixFromJson(cube.at("world"))});
                }
            }
            for (const json &cube : parsed.value("cubes", json::array())) {
                save.cubes.push_back(
                    {.name = cube.at("name").get<std::string>(), .world = MatrixFromJson(cube.at("world"))});
            }
            for (const json &model : parsed.value("models", json::array())) {
                save.models.push_back({.path = PathFromJson(model.at("path").get<std::string>(), root),
                                       .world = MatrixFromJson(model.at("world"))});
            }
            return save;
        } catch (const std::exception &e) {
            return std::unexpected(Error{ErrorCode::DecodeFailed, std::format("{}: {}", name, e.what())});
        }
    }

    std::expected<void, std::string> WriteSandboxSave(const fs::path &file, const SandboxSave &save,
                                                      const fs::path &root) {
        json grid{{"count", save.gridCount}, {"mode", save.gridMode}, {"cubes", json::array()}};
        for (const SavedGridCube &cube : save.gridCubes) {
            grid["cubes"].push_back({{"index", cube.index}, {"world", MatrixToJson(cube.world)}});
        }
        json cubes = json::array();
        for (const SavedCube &cube : save.cubes) {
            cubes.push_back({{"name", cube.name}, {"world", MatrixToJson(cube.world)}});
        }
        json models = json::array();
        for (const SavedModel &model : save.models) {
            models.push_back({{"path", PathToJson(model.path, root)}, {"world", MatrixToJson(model.world)}});
        }
        const json document{{"version", kSaveVersion},
                            {"grid", std::move(grid)},
                            {"cubes", std::move(cubes)},
                            {"models", std::move(models)}};

        const std::string name = core::ToUtf8(file.native());
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        if (ec) {
            return std::unexpected(std::format("{}: cannot create the folder: {}", name, ec.message()));
        }
        fs::path temporary = file;
        temporary += L".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream << document.dump(2) << '\n';
            if (!stream) {
                return std::unexpected(std::format("{}: cannot write", core::ToUtf8(temporary.native())));
            }
        }

        fs::rename(temporary, file, ec);
        if (ec) {
            return std::unexpected(std::format("{}: cannot replace it: {}", name, ec.message()));
        }
        return {};
    }
} // namespace sandbox
