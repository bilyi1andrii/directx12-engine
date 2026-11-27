#include "SmokeTestScript.h"

#include "core/Log.h"

using core::LogInfo;

namespace sandbox {
    SmokeTestScript::SmokeTestScript(uint32_t frames) : m_frames(frames) {
        LogInfo("Smoke test: rendering {} frames", m_frames);
    }

    void SmokeTestScript::AddModels(SandboxModels &models, const std::optional<std::filesystem::path> &modelPath) {
        if (modelPath) {
            models.Add(*modelPath);
        }
    }

    scene::ObjectId SmokeTestScript::InitialSelection(const scene::Scene &scene) {
        return scene.LightObjects().front();
    }

    void SmokeTestScript::AfterFrame(uint64_t framesRendered, core::Window &window,
                                     render::RendererSettings &settings) const {
        if (framesRendered == m_frames / 3 || framesRendered == 2 * m_frames / 3) {
            window.ToggleFullscreen();
        }
        if (framesRendered == m_frames / 2) {
            settings.drawPath = render::DrawPath::NonInstanced;
            LogInfo("Smoke test: draw path non-instanced from frame {}", framesRendered + 1);
        }
        if (framesRendered == m_frames) {
            window.Close();
        }
    }
} // namespace sandbox
