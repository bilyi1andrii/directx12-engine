// The sandbox: a window with the cube grid, models and lights to look around, select and move, and the debug UI. It
// saves the scene when it closes.

#include <format>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FrameTimeAverages.h"
#include "SandboxLights.h"
#include "SandboxOptions.h"
#include "SandboxScene.h"
#include "assets/AssetCache.h"
#include "common/AppChecks.h"
#include "common/AppMain.h"
#include "common/AppShell.h"
#include "common/GridSetup.h"
#include "core/File.h"
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

namespace fs = std::filesystem;
namespace chrono = std::chrono;

using core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    void LogSettingsChanges(const render::RendererSettings &before, const render::RendererSettings &after) {
        if (after.drawPath != before.drawPath) {
            LogInfo("Draw path: {}", after.drawPath == render::DrawPath::Instanced ? "instanced" : "non-instanced");
        }
        if (after.vsync != before.vsync) {
            LogInfo("VSync {}", after.vsync ? "on" : "off");
        }
    }

    apps::RunResult RunSandbox(const SandboxOptions &options, rhi::ValidationStats &validation) {
        core::Timer initTimer;

        apps::AppShell shell({.title = L"D3D12 Engine", .ui = true}, validation);
        core::Input &input = shell.Input();
        core::Window &window = shell.Window();
        ui::DebugUI &ui = shell.Ui();
        render::Renderer &renderer = shell.Renderer();

        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), window.Width(), window.Height(),
                renderer.TearingSupported() ? "supported" : "not supported");

        const std::optional<uint32_t> smokeTestFrames = options.smokeTestFrames;
        if (smokeTestFrames) {
            LogInfo("Smoke test: rendering {} frames", *smokeTestFrames);
        }

        const bool keepScene = !smokeTestFrames && !options.modelPath;
        const std::optional<sandbox::SandboxSave> saved = keepScene ? sandbox::ReadSave() : std::nullopt;

        scene::Camera camera;
        camera.position = {3.0f, 4.0f, -8.0f};
        scene::LookAt(camera, {0.0f, 0.0f, 0.0f});
        camera.fovDegrees = 60.0f;
        scene::CameraController cameraController;

        render::RenderList renderList;
        render::LightSet lights;

        assets::AssetCache assets(renderer.Resources());
        scene::Scene scene;

        const fs::path gridTexturePath =
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
        std::vector<render::StaticListHandle> staticLists;
        if (grid.mode == apps::GridMode::Static) {
            render::RenderList staticObjects;
            scene::Extract(scene, staticObjects, scene::Mobility::Static);
            staticLists.push_back(renderer.Resources().CreateStaticList(staticObjects, L"Grid/Instances"));
        }

        std::vector<sandbox::PlacedModel> models;
        if (options.modelPath) {
            if (std::optional<sandbox::PlacedModel> placed =
                    sandbox::AddModel(scene, assets, *options.modelPath, 0.0f, 0.0f)) {
                models.push_back(std::move(*placed));
            }
        } else if (!smokeTestFrames) {
            std::set<fs::path> savedPaths;
            if (saved) {
                for (const sandbox::SavedModel &model : saved->models) {
                    savedPaths.insert(model.path);
                    if (std::optional<sandbox::PlacedModel> placed =
                            sandbox::AddModel(scene, assets, model.path, 0.0f, 0.0f, model.world)) {
                        models.push_back(std::move(*placed));
                    }
                }
            }
            std::vector<fs::path> newModels = sandbox::FindAssetModels();
            std::erase_if(newModels,
                          [&savedPaths](const fs::path &path) { return savedPaths.contains(path.lexically_normal()); });
            const std::optional<float> firstX = sandbox::NextFreeModelX(scene, models);
            std::ranges::move(sandbox::AddModels(scene, assets, newModels, firstX), std::back_inserter(models));
        }
        const size_t modelsShown = models.size();
        float nextModelX =
            std::max(sandbox::kModelSpacing, sandbox::NextFreeModelX(scene, models).value_or(sandbox::kModelSpacing));

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
        const float cubesZ = modelsShown > 0 ? sandbox::kModelSize / 2 + 3.0f : 0.0f;
        const std::array namedCubes{
            addCube("Orange cube", assets.SolidColor(230, 120, 30), -5.0f, cubesZ),
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
        sandbox::AddDefaultLights(scene, assets, cubesZ);

        std::optional<scene::ObjectId> selected;
        if (smokeTestFrames) {
            selected = scene.LightObjects().front();
        }

        render::RendererSettings settings;

        const std::string sceneInfo = std::format("grid {} x {}, {}", grid.side, grid.side, apps::ToString(grid.mode));

        core::FrameTimer frameTimer;
        sandbox::FrameTimeAverages frameTimes;
        uint64_t framesRendered = 0;
        while (window.PumpMessages()) {
            frameTimer.Tick();

            // A smoke test keeps rendering while minimized, or it would never reach its frame count
            if (window.IsMinimized() && !smokeTestFrames) {
                input.EndFrame();
                ::WaitMessage();
                continue;
            }

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

            const DirectX::XMFLOAT2 viewportPx{static_cast<float>(window.Width()), static_cast<float>(window.Height())};
            const render::FrameView frameView = scene::MakeFrameView(camera, viewportPx.x / viewportPx.y);
            const auto pickRayAt = [&camera, viewportPx](int32_t x, int32_t y) {
                return scene::MakePickRay(camera, {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f},
                                          viewportPx);
            };
            if (spinningCube) {
                using namespace DirectX;
                const float angle =
                    XMConvertToRadians(static_cast<float>(std::fmod(frameTimer.TotalSeconds() * 90.0, 360.0)));
                const XMFLOAT3 position = scene.ObjectPosition(*spinningCube);
                XMFLOAT4X4 world{};
                XMStoreFloat4x4(&world, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle) *
                                            XMMatrixTranslation(position.x, position.y, position.z));
                scene.SetObjectWorld(*spinningCube, world);
            }

            if (selected && scene.Object(*selected).mobility == scene::Mobility::Movable) {
                DirectX::XMFLOAT4X4 gizmoWorld = scene.Object(*selected).world;
                if (ui.TransformGizmo(frameView.view, frameView.projection, gizmoWorld)) {
                    scene.SetObjectWorld(*selected, gizmoWorld);
                }
            }
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
            if (ui::ShowLightsWindow(scene.Environment(),
                                     selected ? std::string_view(scene.Object(*selected).name) : "",
                                     selected ? scene.Light(*selected) : nullptr)) {
                sandbox::RecolorMarker(scene, assets, *selected);
            }
            ImDrawData *const uiDrawData = ui.Render();
            LogSettingsChanges(previousSettings, settings);

            input.EndFrame();

            for (const apps::DroppedFile &drop : shell.TakeDroppedFiles()) {
                const scene::Ray ray = pickRayAt(drop.x, drop.y);
                sandbox::LoadDroppedFile(drop.path, scene::IntersectHorizontalPlane(ray, apps::kGridTop),
                                         renderer.Resources(), assets, scene, nextModelX, grid, models);
            }

            renderer.BeginFrame(window.Width(), window.Height(), settings);
            const auto recordStart = chrono::steady_clock::now();
            renderList.Clear();
            scene::Extract(scene, renderList, scene::Mobility::Movable);
            scene::Extract(scene, lights);
            renderer.Render(frameView, lights, renderList, staticLists, uiDrawData);
            const chrono::duration<double, std::milli> recordMs = chrono::steady_clock::now() - recordStart;
            frameTimes.Add(frameTimer.DeltaSeconds(), recordMs.count(), renderer.GpuTimes());
            renderer.EndFrame();
            ++framesRendered;

            if (smokeTestFrames) {
                const uint32_t frames = *smokeTestFrames;
                if (framesRendered == frames / 3 || framesRendered == 2 * frames / 3) {
                    window.ToggleFullscreen();
                }
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
            const fs::path file = sandbox::SavePath();
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
    std::optional<SandboxOptions> options;
    apps::RunResult run;
    return apps::AppMain({
        .usage = kSandboxUsage,
        .errorDialog = true,
        .parse = [&options](std::span<const std::string> args) -> std::expected<apps::RunKind, std::string> {
            std::expected<SandboxOptions, std::string> parsed = ParseSandboxOptions(args);
            if (!parsed) {
                return std::unexpected(std::move(parsed.error()));
            }
            options = std::move(*parsed);
            return options->smokeTestFrames ? apps::RunKind::Unattended : apps::RunKind::Interactive;
        },
        .run = [&](rhi::ValidationStats &validation) { run = RunSandbox(*options, validation); },
        .finish =
            [&](const rhi::ValidationStats &validation) {
                const int exitCode = options->smokeTestFrames
                                         ? apps::SmokeTestExitCode(*options->smokeTestFrames, run, validation)
                                         : run.exitCode;
                LogInfo("Exiting with code {}", exitCode);
                return exitCode;
            },
    });
}
