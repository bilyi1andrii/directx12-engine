#ifndef D3D12_ENGINE_SMOKETESTSCRIPT_H
#define D3D12_ENGINE_SMOKETESTSCRIPT_H

// What the sandbox does in a smoke test: it renders a fixed number of frames of the real sandbox, toggles fullscreen,
// switches to the non-instanced draw path halfway and closes. It skips the assets/ models to start fast.

#include <cstdint>
#include <filesystem>
#include <optional>

#include "SandboxModels.h"
#include "core/Window.h"
#include "renderdata/RendererSettings.h"
#include "scene/Scene.h"

namespace sandbox {
    class SmokeTestScript {
    public:
        explicit SmokeTestScript(uint32_t frames);

        [[nodiscard]] uint32_t Frames() const noexcept { return m_frames; }

        static void AddModels(SandboxModels &models, const std::optional<std::filesystem::path> &modelPath);

        [[nodiscard]] static scene::ObjectId InitialSelection(const scene::Scene &scene);

        void AfterFrame(uint64_t framesRendered, core::Window &window, render::RendererSettings &settings) const;

    private:
        uint32_t m_frames;
    };
} // namespace sandbox

#endif // D3D12_ENGINE_SMOKETESTSCRIPT_H
