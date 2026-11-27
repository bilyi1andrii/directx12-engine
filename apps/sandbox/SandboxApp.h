#ifndef D3D12_ENGINE_SANDBOXAPP_H
#define D3D12_ENGINE_SANDBOXAPP_H

// The sandbox: the grid, models, cubes and lights to fly around, select and move, with the debug UI. Each frame runs
// in a fixed order: keys, camera, edits, windows, dropped files, then rendering. It saves the scene when it closes.

#include <cstdint>
#include <optional>
#include <string>

#include "FrameTimeAverages.h"
#include "SandboxGrid.h"
#include "SandboxModels.h"
#include "SandboxOptions.h"
#include "SandboxScene.h"
#include "SmokeTestScript.h"
#include "assets/AssetCache.h"
#include "common/AppChecks.h"
#include "common/AppShell.h"
#include "core/Timer.h"
#include "renderdata/FrameView.h"
#include "renderdata/Lights.h"
#include "renderdata/RenderList.h"
#include "renderdata/RendererSettings.h"
#include "sandbox/SceneSave.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"
#include "scene/Picking.h"
#include "scene/Scene.h"

struct ImDrawData;

namespace sandbox {
    class SandboxApp {
    public:
        SandboxApp(apps::AppShell &shell, const SandboxOptions &options);

        SandboxApp(const SandboxApp &) = delete;
        SandboxApp &operator=(const SandboxApp &) = delete;
        SandboxApp(SandboxApp &&) = delete;
        SandboxApp &operator=(SandboxApp &&) = delete;

        [[nodiscard]] apps::RunResult Run();

    private:
        struct InputFocus {
            bool keyboard = true;
            bool mouse = true;
        };

        SandboxApp(apps::AppShell &shell, const SandboxOptions &options, const std::optional<SandboxSave> &saved);

        [[nodiscard]] bool WaitWhileMinimized();
        [[nodiscard]] InputFocus BeginUiFrame();
        void HandleAppKeys(InputFocus focus);
        [[nodiscard]] render::FrameView UpdateCamera(InputFocus focus);
        void SpinCube();
        void EditSelection(const render::FrameView &view, InputFocus focus);
        [[nodiscard]] ImDrawData *ShowWindows();
        void LoadDroppedFiles();
        void RenderFrame(const render::FrameView &view, ImDrawData *uiDrawData);

        [[nodiscard]] scene::Ray PickRay(int32_t x, int32_t y) const;

        apps::AppShell &m_shell;
        std::optional<SmokeTestScript> m_smokeTest;
        bool m_keepScene;
        scene::Camera m_camera;
        scene::CameraController m_cameraController;
        render::RenderList m_renderList;
        render::LightSet m_lights;
        assets::AssetCache m_assets;
        scene::Scene m_scene;
        SandboxGrid m_grid;
        SandboxModels m_models;
        SandboxCubes m_cubes;
        std::optional<scene::ObjectId> m_selected;
        render::RendererSettings m_settings;
        std::string m_sceneInfo;
        core::FrameTimer m_frameTimer;
        FrameTimeAverages m_frameTimes;
        uint64_t m_framesRendered = 0;
    };
} // namespace sandbox

#endif // D3D12_ENGINE_SANDBOXAPP_H
