#include "SandboxApp.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string_view>

#include <DirectXMath.h>
#include <Windows.h>

#include "SandboxLights.h"
#include "SandboxSaving.h"
#include "assets/ModelImporter.h"
#include "common/GridSetup.h"
#include "core/Input.h"
#include "core/Log.h"
#include "core/Utf8.h"
#include "core/Window.h"
#include "render/Renderer.h"
#include "ui/DebugUI.h"
#include "ui/LightsWindow.h"

namespace fs = std::filesystem;
namespace chrono = std::chrono;

using core::LogInfo;

namespace sandbox {
    namespace {
        scene::Camera MakeStartCamera() {
            scene::Camera camera;
            camera.position = {3.0f, 4.0f, -8.0f};
            scene::LookAt(camera, {0.0f, 0.0f, 0.0f});
            camera.fovDegrees = 60.0f;
            return camera;
        }

        std::optional<SmokeTestScript> MakeSmokeTest(const SandboxOptions &options) {
            if (!options.smokeTestFrames) {
                return std::nullopt;
            }
            return SmokeTestScript(*options.smokeTestFrames);
        }

        DirectX::XMFLOAT2 ViewportPx(const core::Window &window) {
            return {static_cast<float>(window.Width()), static_cast<float>(window.Height())};
        }

        void LogSettingsChanges(const render::RendererSettings &before, const render::RendererSettings &after) {
            if (after.drawPath != before.drawPath) {
                LogInfo("Draw path: {}", after.drawPath == render::DrawPath::Instanced ? "instanced" : "non-instanced");
            }
            if (after.vsync != before.vsync) {
                LogInfo("VSync {}", after.vsync ? "on" : "off");
            }
        }

        void LoadDroppedFile(const fs::path &path, const std::optional<DirectX::XMFLOAT3> &dropPoint, SandboxGrid &grid,
                             SandboxModels &models) {
            LogInfo("Dropped {}: loading...", core::ToUtf8(path.filename().wstring()));
            if (assets::IsModelFile(path)) {
                models.AddDropped(path, dropPoint);
            } else {
                grid.LoadDroppedTexture(path);
            }
        }
    } // namespace

    SandboxApp::SandboxApp(apps::AppShell &shell, const SandboxOptions &options)
        : SandboxApp(shell, options, UsesSave(options) ? ReadSave() : std::nullopt) {}

    SandboxApp::SandboxApp(apps::AppShell &shell, const SandboxOptions &options,
                           const std::optional<SandboxSave> &saved)
        : m_shell(shell), m_smokeTest(MakeSmokeTest(options)), m_keepScene(UsesSave(options)),
          m_camera(MakeStartCamera()), m_assets(shell.Renderer().Resources()),
          m_grid(m_scene, m_assets, shell.Renderer().Resources(), options, saved), m_models(m_scene, m_assets),
          m_sceneInfo(m_grid.Describe()) {
        if (m_smokeTest) {
            SmokeTestScript::AddModels(m_models, options.modelPath);
        } else {
            AddDefaultModels(m_models, options, saved);
        }
        m_cubes = AddCubesAndLights(m_scene, m_assets, !m_models.Empty());
        if (saved) {
            RestoreNamedCubes(m_scene, m_cubes.named, *saved);
        }
        if (m_smokeTest) {
            m_selected = SmokeTestScript::InitialSelection(m_scene);
        }
    }

    apps::RunResult SandboxApp::Run() {
        core::Window &window = m_shell.Window();
        while (window.PumpMessages()) {
            m_frameTimer.Tick();
            if (WaitWhileMinimized()) {
                continue;
            }

            const InputFocus focus = BeginUiFrame();
            const render::RendererSettings previousSettings = m_settings;
            HandleAppKeys(focus);
            const render::FrameView view = UpdateCamera(focus);
            SpinCube();
            EditSelection(view, focus);
            ImDrawData *const uiDrawData = ShowWindows();
            LogSettingsChanges(previousSettings, m_settings);
            m_shell.Input().EndFrame();

            LoadDroppedFiles();
            RenderFrame(view, uiDrawData);
            ++m_framesRendered;

            if (m_smokeTest) {
                m_smokeTest->AfterFrame(m_framesRendered, window, m_settings);
            }
        }

        if (m_keepScene) {
            WriteSave(m_scene, m_grid, m_cubes.named, m_models);
        }
        return {.exitCode = window.ExitCode(), .framesRendered = m_framesRendered};
    }

    bool SandboxApp::WaitWhileMinimized() {
        // A smoke test keeps rendering while minimized, or it would never reach its frame count
        if (m_shell.Window().IsMinimized() && !m_smokeTest) {
            m_shell.Input().EndFrame();
            ::WaitMessage();
            return true;
        }
        return false;
    }

    SandboxApp::InputFocus SandboxApp::BeginUiFrame() {
        ui::DebugUI &ui = m_shell.Ui();
        ui.NewFrame();
        return {.keyboard = !ui.WantsKeyboard(), .mouse = !ui.WantsMouse()};
    }

    void SandboxApp::HandleAppKeys(InputFocus focus) {
        if (!focus.keyboard) {
            return;
        }
        const core::Input &input = m_shell.Input();
        core::Window &window = m_shell.Window();
        if (input.WasKeyPressed(VK_ESCAPE)) {
            window.Close();
        }
        if (input.WasKeyPressed('V')) {
            m_settings.vsync = !m_settings.vsync;
        }
        if (input.WasKeyPressed('I')) {
            m_settings.drawPath = (m_settings.drawPath == render::DrawPath::Instanced) ? render::DrawPath::NonInstanced
                                                                                       : render::DrawPath::Instanced;
        }
        if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
            window.ToggleFullscreen();
        }
    }

    render::FrameView SandboxApp::UpdateCamera(InputFocus focus) {
        m_cameraController.Update(m_camera, m_shell.Input(), m_frameTimer.DeltaSeconds(),
                                  {.mouse = !m_shell.Ui().WantsMouseForCamera(), .keyboard = focus.keyboard});
        const DirectX::XMFLOAT2 viewportPx = ViewportPx(m_shell.Window());
        return scene::MakeFrameView(m_camera, viewportPx.x / viewportPx.y);
    }

    void SandboxApp::SpinCube() {
        if (!m_cubes.spinning) {
            return;
        }
        using namespace DirectX;
        const float angle =
            XMConvertToRadians(static_cast<float>(std::fmod(m_frameTimer.TotalSeconds() * 90.0, 360.0)));
        const XMFLOAT3 position = m_scene.ObjectPosition(*m_cubes.spinning);
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle) *
                                    XMMatrixTranslation(position.x, position.y, position.z));
        m_scene.SetObjectWorld(*m_cubes.spinning, world);
    }

    void SandboxApp::EditSelection(const render::FrameView &view, InputFocus focus) {
        ui::DebugUI &ui = m_shell.Ui();
        if (m_selected && m_scene.Object(*m_selected).mobility == scene::Mobility::Movable) {
            DirectX::XMFLOAT4X4 gizmoWorld = m_scene.Object(*m_selected).world;
            if (ui.TransformGizmo(view.view, view.projection, gizmoWorld)) {
                m_scene.SetObjectWorld(*m_selected, gizmoWorld);
            }
        }
        const core::Input &input = m_shell.Input();
        if (focus.mouse && !ui.GizmoWantsMouse() && input.WasKeyPressed(VK_LBUTTON)) {
            const core::Int2 mouse = input.MousePosition();
            m_selected = scene::Pick(m_scene, PickRay(mouse.x, mouse.y));
            LogInfo("Selected: {}", m_selected ? std::string_view(m_scene.Object(*m_selected).name) : "nothing");
        }
    }

    ImDrawData *SandboxApp::ShowWindows() {
        ui::DebugUI &ui = m_shell.Ui();
        ui.ShowRendererWindow({.fps = m_frameTimer.Fps(),
                               .cpuRecordMs = m_frameTimes.CpuRecordMs(),
                               .gpuTotalMs = m_frameTimes.GpuTotalMs(),
                               .gpuPasses = m_frameTimes.GpuPasses(),
                               .objectCount = m_scene.Objects().size(),
                               .sceneInfo = m_sceneInfo,
                               .cameraPosition = m_camera.position,
                               .cameraSpeedScale = m_cameraController.SpeedScale()},
                              m_settings);
        if (m_selected) {
            const scene::SceneObject &object = m_scene.Object(*m_selected);
            DirectX::XMFLOAT3 position = m_scene.ObjectPosition(*m_selected);
            if (ui.ShowSelectionWindow(object.name, position, object.mobility == scene::Mobility::Movable)) {
                m_scene.SetObjectPosition(*m_selected, position);
            }
        } else {
            ui.ShowEmptySelectionWindow();
        }
        if (ui::ShowLightsWindow(m_scene.Environment(),
                                 m_selected ? std::string_view(m_scene.Object(*m_selected).name) : "",
                                 m_selected ? m_scene.Light(*m_selected) : nullptr)) {
            RecolorMarker(m_scene, m_assets, *m_selected);
        }
        return ui.Render();
    }

    void SandboxApp::LoadDroppedFiles() {
        for (const apps::DroppedFile &drop : m_shell.TakeDroppedFiles()) {
            LoadDroppedFile(drop.path, scene::IntersectHorizontalPlane(PickRay(drop.x, drop.y), apps::kGridTop), m_grid,
                            m_models);
        }
    }

    void SandboxApp::RenderFrame(const render::FrameView &view, ImDrawData *uiDrawData) {
        render::Renderer &renderer = m_shell.Renderer();
        const core::Window &window = m_shell.Window();
        renderer.BeginFrame(window.Width(), window.Height(), m_settings);
        const auto recordStart = chrono::steady_clock::now();
        m_renderList.Clear();
        scene::Extract(m_scene, m_renderList, scene::Mobility::Movable);
        scene::Extract(m_scene, m_lights);
        renderer.Render(view, m_lights, m_renderList, m_grid.StaticLists(), uiDrawData);
        const chrono::duration<double, std::milli> recordMs = chrono::steady_clock::now() - recordStart;
        m_frameTimes.Add(m_frameTimer.DeltaSeconds(), recordMs.count(), renderer.GpuTimes());
        renderer.EndFrame();
    }

    scene::Ray SandboxApp::PickRay(int32_t x, int32_t y) const {
        return scene::MakePickRay(m_camera, {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f},
                                  ViewportPx(m_shell.Window()));
    }
} // namespace sandbox
