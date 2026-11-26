#include <format>

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SandboxLights.h"
#include "SandboxOptions.h"
#include "SandboxScene.h"
#include "assets/AssetCache.h"
#include "common/AppChecks.h"
#include "common/GridSetup.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/File.h"
#include "core/IntervalAverage.h"
#include "core/Log.h"
#include "core/Timer.h"
#include "core/Utf8.h"
#include "core/Window.h"
#include "core/Input.h"
#include "rhi/AgilitySdk.h"
#include "rhi/ValidationStats.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/CameraController.h"
#include "scene/Picking.h"
#include "scene/Scene.h"
#include "ui/DebugUI.h"
#include "ui/LightsWindow.h"

using core::LogError;
using core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    // Logs what the renderer window or a shortcut changed this frame
    void LogSettingsChanges(const render::RendererSettings &before, const render::RendererSettings &after) {
        if (after.drawPath != before.drawPath) {
            LogInfo("Draw path: {}", after.drawPath == render::DrawPath::Instanced ? "instanced" : "non-instanced");
        }
        if (after.vsync != before.vsync) {
            LogInfo("VSync {}", after.vsync ? "on" : "off");
        }
    }

    class FrameTimeAverages {
    public:
        void Add(double deltaSeconds, double cpuRecordMs, const std::optional<rhi::GpuFrameTimes> &gpu) {
            m_cpuRecord.Add(cpuRecordMs, deltaSeconds);
            if (!gpu) {
                return;
            }
            m_gpuTotal.Add(gpu->totalMs, deltaSeconds);

            const std::span<const rhi::GpuScopeTime> passes = gpu->Scopes();
            if (passes.size() != m_passCount) {
                for (core::IntervalAverage &average : m_passAverages) {
                    average.Reset();
                }
                m_passCount = passes.size();
            }
            for (size_t i = 0; i < passes.size(); ++i) {
                m_passAverages[i].Add(passes[i].ms, deltaSeconds);
                m_passes[i] = {.name = passes[i].name, .ms = m_passAverages[i].Value()};
            }
        }

        [[nodiscard]] double CpuRecordMs() const noexcept { return m_cpuRecord.Value(); }
        [[nodiscard]] double GpuTotalMs() const noexcept { return m_gpuTotal.Value(); }
        [[nodiscard]] std::span<const ui::PassTime> GpuPasses() const noexcept {
            return std::span(m_passes).first(m_passCount);
        }

    private:
        core::IntervalAverage m_cpuRecord;
        core::IntervalAverage m_gpuTotal;
        std::array<core::IntervalAverage, rhi::kMaxGpuScopes> m_passAverages;
        std::array<ui::PassTime, rhi::kMaxGpuScopes> m_passes{};
        size_t m_passCount = 0;
    };

    // A dropped file and its drop point (client pixels, (0, 0) top-left)
    struct DroppedFile {
        std::filesystem::path path;
        int32_t x = 0;
        int32_t y = 0;
    };

    // Creates the window and the renderer and runs the game loop until the window closes
    // (in a smoke test: after the requested number of frames). Everything it creates is
    // destroyed before it returns, so `validation` is complete afterwards.
    apps::RunResult RunSandbox(const SandboxOptions &options, rhi::ValidationStats &validation) {
        core::Timer initTimer;
        initTimer.Start();

        core::Input input;
        std::vector<DroppedFile> droppedFiles;

        core::Window window({.title = L"D3D12 Engine", .width = 1280, .height = 720});

        // After the window (its backend needs the HWND), before the renderer (the renderer's ImGui backend needs
        // the context, also while the renderer is destroyed)
        ui::DebugUI ui(window);

        window.SetCallbacks({
            .onMessage =
                [&input](const core::WindowMessage &message) {
                    // ImGui first. DebugUI::HandleMessage is static and does nothing once `ui` is gone, so this
                    // stays safe while the window, which outlives `ui`, is destroyed.
                    const bool handledByUi = ui::DebugUI::HandleMessage(message);
                    input.HandleMessage(message);
                    // Only true for WM_SETCURSOR over ImGui (and IME text): the window must not reset the cursor
                    return handledByUi;
                },
            .onFilesDropped =
                [&droppedFiles](std::span<const std::filesystem::path> paths, int32_t x, int32_t y) {
                    for (const std::filesystem::path &path : paths) {
                        droppedFiles.push_back({.path = path, .x = x, .y = y});
                    }
                },
        });

        render::Renderer renderer(window.Handle(), window.Width(), window.Height(), validation);
        renderer.EnableUi();

        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), window.Width(), window.Height(),
                renderer.TearingSupported() ? "supported" : "not supported");

        const std::optional<uint32_t> smokeTestFrames = options.smokeTestFrames;
        if (smokeTestFrames) {
            LogInfo("Smoke test: rendering {} frames", *smokeTestFrames);
        }

        const bool keepScene = !smokeTestFrames && !options.modelPath;
        const std::optional<sandbox::SandboxSave> saved = keepScene ? sandbox::ReadSave() : std::nullopt;

        // Looks at the cube from above and to the side, so three faces are visible
        scene::Camera camera;
        camera.position = {3.0f, 4.0f, -8.0f};
        scene::LookAt(camera, {0.0f, 0.0f, 0.0f});
        camera.fovDegrees = 60.0f;
        scene::CameraController cameraController;

        render::RenderList renderList; // this frame's movable objects
        render::LightSet lights;       // this frame's lights

        // Destroyed before the renderer, which owns what the cache created
        assets::AssetCache assets(renderer.Resources());
        scene::Scene scene;

        // The textured grid (--count, --grid)
        // Without --texture, the grid uses the sandbox's texture from data/
        const std::filesystem::path gridTexturePath =
            options.texturePath.value_or(core::ExecutableDirectory() / sandbox::kDefaultGridTexture);
        const uint32_t gridCount =
            options.gridCount.value_or(saved && saved->gridCount > 0 ? saved->gridCount : apps::kDefaultGridCount);
        const apps::GridMode gridMode = options.gridMode.value_or(
            saved ? apps::ParseGridMode(saved->gridMode).value_or(apps::GridMode::Dynamic) : apps::GridMode::Dynamic);
        const sandbox::GridObjects grid = sandbox::AddGrid(
            scene, assets, assets.LoadTexture(gridTexturePath, assets::TextureUsage::Diffuse), gridMode, gridCount);
        if (saved && !saved->gridCubes.empty()) {
            if (grid.mode == apps::GridMode::Dynamic && saved->gridCount == gridCount) {
                for (const sandbox::SavedGridCube &cube : saved->gridCubes) {
                    if (cube.index < grid.side * grid.side) {
                        scene.SetObjectWorld({.index = grid.firstObject + cube.index}, cube.world);
                    }
                }
            } else {
                LogInfo("Scene: {} saved grid cubes left out (they belong to a dynamic grid of {} cubes)",
                        saved->gridCubes.size(), saved->gridCount);
            }
        }
        // Static objects are uploaded to the GPU once, here, so any new ones must be added before this point
        std::vector<render::StaticListHandle> staticLists;
        if (grid.mode == apps::GridMode::Static) {
            render::RenderList staticObjects;
            scene::Extract(scene, staticObjects, scene::Mobility::Static);
            staticLists.push_back(renderer.Resources().CreateStaticList(staticObjects, L"Grid/Instances"));
        }

        std::vector<sandbox::PlacedModel> models;
        if (options.modelPath) {
            models = sandbox::AddModels(scene, assets, {&*options.modelPath, 1});
        } else if (!smokeTestFrames) {
            std::vector<std::filesystem::path> newModels = sandbox::FindAssetModels();
            if (saved) {
                for (const sandbox::SavedModel &model : saved->models) {
                    std::erase_if(newModels, [&model](const std::filesystem::path &path) {
                        return path.lexically_normal() == model.path;
                    });
                    if (std::optional<sandbox::PlacedModel> placed =
                            sandbox::AddModel(scene, assets, model.path, 0.0f, 0.0f, model.world)) {
                        models.push_back(std::move(*placed));
                    }
                }
            }
            std::optional<float> firstX;
            for (const sandbox::PlacedModel &model : models) {
                firstX = std::max(firstX.value_or(-FLT_MAX), scene.ObjectPosition(model.id).x + sandbox::kModelSpacing);
            }
            std::ranges::move(sandbox::AddModels(scene, assets, newModels, firstX), std::back_inserter(models));
        }
        const size_t modelsShown = models.size();
        float nextModelX = sandbox::kModelSpacing;
        for (const sandbox::PlacedModel &model : models) {
            nextModelX = std::max(nextModelX, scene.ObjectPosition(model.id).x + sandbox::kModelSpacing);
        }

        // Cubes to select and move: one spinning at the origin (unless models are there) and three plain-coloured ones
        const auto addCube = [&scene, &assets](std::string name, render::TextureHandle texture, float x, float z) {
            DirectX::XMFLOAT4X4 world{};
            DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(x, 0.0f, z));
            return scene.AddObject(std::move(name), assets.CubeModel(texture), world, scene::Mobility::Movable);
        };
        std::optional<scene::ObjectId> spinningCube;
        if (modelsShown == 0) {
            spinningCube = addCube("Spinning cube",
                                   assets.LoadTexture(core::ExecutableDirectory() / sandbox::kSpinningCubeTexture,
                                                      assets::TextureUsage::Diffuse),
                                   0.0f, 0.0f);
        }
        // With models loaded, the cubes stand behind the row so that they do not end up inside a model
        const float cubesZ = modelsShown > 0 ? sandbox::kModelSize / 2 + 3.0f : 0.0f;
        const scene::ObjectId orangeCube = addCube("Orange cube", assets.SolidColor(230, 120, 30), -5.0f, cubesZ);

        const std::array namedCubes{
            orangeCube,
            addCube("Green cube", assets.SolidColor(60, 170, 70), 5.0f, cubesZ),
            addCube("Blue cube", assets.SolidColor(50, 110, 220), 0.0f, cubesZ + 5.0f),
        };
        if (saved) {
            for (const sandbox::SavedCube &savedCube : saved->cubes) {
                for (const scene::ObjectId id : namedCubes) {
                    if (scene.Object(id).name == savedCube.name) {
                        scene.SetObjectWorld(id, savedCube.world);
                    }
                }
            }
        }
        // The lights stand between and behind the cubes
        sandbox::AddDefaultLights(scene, assets, cubesZ);

        std::optional<scene::ObjectId> selected;
        if (smokeTestFrames) {
            selected = scene.LightObjects().front();
        }

        // Edited by the renderer window and the shortcuts I and V; the renderer takes a copy in BeginFrame
        render::RendererSettings settings;

        // For the renderer window; the grid does not change while the sandbox runs
        const std::string sceneInfo = std::format("grid {} x {}, {}", grid.side, grid.side, apps::ToString(grid.mode));

        // Game loop: handle all pending window messages, update, render
        core::FrameTimer frameTimer;
        FrameTimeAverages frameTimes;
        uint64_t framesRendered = 0;
        while (window.PumpMessages()) {
            frameTimer.Tick();

            // A minimized window has nothing to show.
            // WaitMessage() sleeps until the next message.
            // A smoke test keeps rendering, or it would never reach its frame count: the swap chain
            // ignores the 0x0 size, and Present returns DXGI_STATUS_OCCLUDED, which is a success code.
            // Before the UI frame, so that every NewFrame gets its Render.
            if (window.IsMinimized() && !smokeTestFrames) {
                input.EndFrame();
                ::WaitMessage();
                continue;
            }

            // The UI frame starts first: NewFrame decides which input ImGui uses this frame, and the app ignores
            // that input (no camera look while dragging a window, no shortcuts while typing into a widget)
            ui.NewFrame();
            const bool appKeyboard = !ui.WantsKeyboard();
            const bool appMouse = !ui.WantsMouse();

            const render::RendererSettings previousSettings = settings;
            if (appKeyboard) {
                if (input.WasKeyPressed(VK_ESCAPE)) {
                    window.Close();
                }
                if (input.WasKeyPressed('V')) {
                    settings.vsync = !settings.vsync;
                }
                if (input.WasKeyPressed('I')) {
                    settings.drawPath = (settings.drawPath == render::DrawPath::Instanced)
                                            ? render::DrawPath::NonInstanced
                                            : render::DrawPath::Instanced;
                }
                if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
                    window.ToggleFullscreen();
                }
            }

            cameraController.Update(camera, input, frameTimer.DeltaSeconds(),
                                    {.mouse = !ui.WantsMouseForCamera(), .keyboard = appKeyboard});

            // One view for the gizmo, picking and rendering. BeginFrame gives the swap chain the window's size, so
            // the window's aspect ratio is also the rendered one.
            const DirectX::XMFLOAT2 viewportPx{static_cast<float>(window.Width()), static_cast<float>(window.Height())};
            const render::FrameView frameView = scene::MakeFrameView(camera, viewportPx.x / viewportPx.y);
            // The camera ray through the centre of a pixel
            const auto pickRayAt = [&camera, viewportPx](int32_t x, int32_t y) {
                return scene::MakePickRay(camera, {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f},
                                          viewportPx);
            };
            if (spinningCube) {
                using namespace DirectX;
                // 90 degrees per second. fmod in double keeps the angle precise in long runs.
                const float angle =
                    XMConvertToRadians(static_cast<float>(std::fmod(frameTimer.TotalSeconds() * 90.0, 360.0)));
                const XMFLOAT3 position = scene.ObjectPosition(*spinningCube);
                XMFLOAT4X4 world{};
                // Spin first, then move, so it spins around its own centre
                XMStoreFloat4x4(&world, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle) *
                                            XMMatrixTranslation(position.x, position.y, position.z));
                scene.SetObjectWorld(*spinningCube, world);
            }

            // Show the gizmo on the selected object, unless it is static
            if (selected && scene.Object(*selected).mobility == scene::Mobility::Movable) {
                // The whole matrix: the gizmo moves, turns or scales the object around its pivot
                DirectX::XMFLOAT4X4 gizmoWorld = scene.Object(*selected).world;
                if (ui.TransformGizmo(frameView.view, frameView.projection, gizmoWorld)) {
                    scene.SetObjectWorld(*selected, gizmoWorld);
                }
            }
            // A left click on the scene selects the object under the cursor, or clears the selection. After the
            // gizmo, so that a click on one of its handles stays the gizmo's.
            if (appMouse && !ui.GizmoWantsMouse() && input.WasKeyPressed(VK_LBUTTON)) {
                const core::Int2 mouse = input.MousePosition();
                selected = scene::Pick(scene, pickRayAt(mouse.x, mouse.y));
                LogInfo("Selected: {}", selected ? std::string_view(scene.Object(*selected).name) : "nothing");
            }

            ui.ShowRendererWindow({.fps = frameTimer.Fps(),
                                   .cpuRecordMs = frameTimes.CpuRecordMs(),
                                   .gpuTotalMs = frameTimes.GpuTotalMs(),
                                   .gpuPasses = frameTimes.GpuPasses(),
                                   .objectCount = scene.Objects().size(),
                                   .sceneInfo = sceneInfo,
                                   .cameraPosition = camera.position,
                                   .cameraSpeedScale = cameraController.SpeedScale()},
                                  settings);
            if (selected) {
                const scene::SceneObject &object = scene.Object(*selected);
                DirectX::XMFLOAT3 position = scene.ObjectPosition(*selected);
                if (ui.ShowSelectionWindow(object.name, position, object.mobility == scene::Mobility::Movable)) {
                    scene.SetObjectPosition(*selected, position);
                }
            } else {
                ui.ShowEmptySelectionWindow();
            }
            // Lights are scene data: the window edits them in place, and this frame's Extract passes them on
            if (ui::ShowLightsWindow(scene.Environment(),
                                     selected ? std::string_view(scene.Object(*selected).name) : "",
                                     selected ? scene.Light(*selected) : nullptr)) {
                sandbox::RecolorMarker(scene, assets, *selected);
            }
            ImDrawData *const uiDrawData = ui.Render();
            LogSettingsChanges(previousSettings, settings);

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // Between frames: EndFrame submits their uploads before the frame
            for (const DroppedFile &drop : std::exchange(droppedFiles, {})) {
                const scene::Ray ray = pickRayAt(drop.x, drop.y);
                sandbox::LoadDroppedFile(drop.path, scene::IntersectHorizontalPlane(ray, apps::kGridTop),
                                         renderer.Resources(), assets, scene, nextModelX, grid, models);
            }

            renderer.BeginFrame(window.Width(), window.Height(), settings);
            const auto recordStart = std::chrono::steady_clock::now();
            renderList.Clear();
            scene::Extract(scene, renderList, scene::Mobility::Movable);
            scene::Extract(scene, lights);
            renderer.Render(frameView, lights, renderList, staticLists, uiDrawData); // the UI on top of the scene
            const std::chrono::duration<double, std::milli> recordMs = std::chrono::steady_clock::now() - recordStart;
            frameTimes.Add(frameTimer.DeltaSeconds(), recordMs.count(), renderer.GpuTimes());
            renderer.EndFrame();
            ++framesRendered;

            if (smokeTestFrames) {
                const uint32_t frames = *smokeTestFrames;
                // Two fullscreen toggles exercise the resize path: flush, ResizeBuffers, new RTVs, a rebuilt
                // frame graph and the deferred release of its old depth texture
                if (frames >= 3 && (framesRendered == frames / 3 || framesRendered == 2 * frames / 3)) {
                    window.ToggleFullscreen();
                }
                // The second half runs on the other draw path, so both stay covered by the smoke test
                if (framesRendered == frames / 2) {
                    settings.drawPath = render::DrawPath::NonInstanced;
                    LogInfo("Smoke test: draw path non-instanced from frame {}", framesRendered + 1);
                }
                if (framesRendered == frames) {
                    window.Close();
                }
            }
        }

        if (keepScene) {
            const std::filesystem::path file = sandbox::SavePath();
            if (const auto written = sandbox::WriteSandboxSave(
                    file, sandbox::MakeSave(scene, grid, gridCount, namedCubes, models), sandbox::RepositoryRoot())) {
                LogInfo("Scene saved: {}", core::ToUtf8(file.native()));
            } else {
                core::LogWarning("Scene not saved: {}", written.error());
            }
        }

        return {.exitCode = window.ExitCode(), .framesRendered = framesRendered};
    }
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    // Known before anything that can fail later; the catch below needs it
    bool smokeTest = false;
    try {
        const std::expected<SandboxOptions, std::string> options = ParseSandboxOptions(core::CommandLineArgs());
        if (!options) {
            // No dialog: a wrong command line usually comes from a script
            LogError("{}", options.error());
            LogInfo("{}", kSandboxUsage);
            return apps::kExitBadCommandLine;
        }
        smokeTest = options->smokeTestFrames.has_value();
        if (smokeTest) {
            // A failed ImGui or D3D12MA assert must end the run, not wait at a dialog until ctest's timeout
            apps::RouteCrtAssertsToStderr();
        }

        // For the whole run: image files are decoded through WIC, which needs COM
        const core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        rhi::ValidationStats validation;
        const apps::RunResult run = RunSandbox(*options, validation);
        apps::LogValidationSummary(validation);

        const int exitCode =
            smokeTest ? apps::SmokeTestExitCode(*options->smokeTestFrames, run, validation) : run.exitCode;
        LogInfo("Exiting with code {}", exitCode);
        return exitCode;
    } catch (const std::exception &e) {
        LogError("{}", e.what());
        // A dialog would block an unattended run until someone closes it
        if (!smokeTest) {
            MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        }
        return -1;
    }
}
