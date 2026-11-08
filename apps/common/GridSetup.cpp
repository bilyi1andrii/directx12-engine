#include "GridSetup.h"

#include <expected>
#include <string>

#include "assets/Primitives.h"
#include "assets/TextureLoader.h"
#include "core/Error.h"
#include "core/Log.h"

namespace engine::apps {
    std::optional<GridMode> ParseGridMode(std::string_view text) noexcept {
        if (text == "static") {
            return GridMode::Static;
        }
        if (text == "dynamic") {
            return GridMode::Dynamic;
        }
        return std::nullopt;
    }

    std::string_view ToString(GridMode mode) noexcept {
        return mode == GridMode::Static ? "static" : "dynamic";
    }

    uint32_t GridSide(uint32_t count) noexcept {
        // 64-bit, so the square cannot overflow
        uint32_t side = 0;
        while (uint64_t{side} * side < count) {
            ++side;
        }
        return side;
    }

    render::FrameView MakeFrameView(const scene::Camera &camera, float aspectRatio) {
        render::FrameView view{};
        DirectX::XMStoreFloat4x4(&view.view, scene::ViewMatrix(camera));
        DirectX::XMStoreFloat4x4(&view.projection, scene::ProjectionMatrix(camera, aspectRatio));
        view.cameraPosition = camera.position;
        return view;
    }

    render::TextureHandle CreateGridTexture(render::Renderer &renderer,
                                            const std::optional<std::filesystem::path> &path) {
        const std::expected<render::TextureData, core::Error> texture =
            path ? assets::LoadTexture(*path, assets::TextureUsage::Diffuse) : assets::MakeCheckerboard();
        if (!texture) {
            core::LogWarning("Grid texture: {}. Using the grey fallback.", texture.error().message);
            return renderer.UnloadedTexture();
        }
        const std::wstring name = path ? L"Grid/" + path->filename().wstring() : L"Grid/Checkerboard";
        return renderer.CreateTexture(*texture, name);
    }
} // namespace engine::apps
