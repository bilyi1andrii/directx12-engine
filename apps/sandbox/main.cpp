#include <format>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SandboxOptions.h"
#include "SandboxScene.h"
#include "assets/AssetCache.h"
#include "assets/Primitives.h"
#include "common/AppChecks.h"
#include "common/StressScene.h"
#include "core/CommandLine.h"
#include "core/ComScope.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/ThrowIfFailed.h"
#include "core/Timer.h"
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

using engine::core::LogError;
using engine::core::LogInfo;

ENGINE_EXPORT_AGILITY_SDK()

namespace {
    // Where an object is: the translation row of its world matrix
    DirectX::XMFLOAT3 ObjectPosition(const engine::scene::SceneObject &object) {
        return {object.world._41, object.world._42, object.world._43};
    }

    // Moves an object to `position` and keeps its rotation and scale
    void MoveObject(engine::scene::Scene &scene, engine::scene::ObjectId id, const DirectX::XMFLOAT3 &position) {
        DirectX::XMFLOAT4X4 world = scene.Object(id).world;
        world._41 = position.x;
        world._42 = position.y;
        world._43 = position.z;
        scene.SetObjectWorld(id, world);
    }

    // Logs what the renderer window or a shortcut changed this frame
    void LogSettingsChanges(const engine::render::RendererSettings &before,
                            const engine::render::RendererSettings &after) {
        if (after.drawPath != before.drawPath) {
            LogInfo("Draw path: {}",
                    after.drawPath == engine::render::DrawPath::Instanced ? "instanced" : "non-instanced");
        }
        if (after.vsync != before.vsync) {
            LogInfo("VSync {}", after.vsync ? "on" : "off");
        }
    }

    // A dropped file and its drop point (client pixels, (0, 0) top-left)
    struct DroppedFile {
        std::filesystem::path path;
        int32_t x = 0;
        int32_t y = 0;
    };

    // Creates the window and the renderer and runs the game loop until the window closes
    // (in a smoke test: after the requested number of frames). Everything it creates is
    // destroyed before it returns, so `validation` is complete afterwards.
    engine::apps::RunResult RunSandbox(const SandboxOptions &options, engine::rhi::ValidationStats &validation) {
        engine::core::Timer initTimer;
        initTimer.Start();

        engine::core::Input input;
        std::vector<DroppedFile> droppedFiles;

        engine::core::Window window({.title = L"D3D12 Engine", .width = 1280, .height = 720});

        // After the window (its backend needs the HWND), before the renderer (the renderer's ImGui backend needs
        // the context, also while the renderer is destroyed)
        engine::ui::DebugUI ui(window);

        // No onResize: one fullscreen toggle sends several WM_SIZE messages, so the swap chain
        // follows Window::Width()/Height() once per frame instead
        window.SetCallbacks({
            .onMessage =
                [&input](const engine::core::WindowMessage &message) {
                    // ImGui first. DebugUI::HandleMessage is static and does nothing once `ui` is gone, so this
                    // stays safe while the window, which outlives `ui`, is destroyed.
                    const bool handledByUi = engine::ui::DebugUI::HandleMessage(message);
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

        engine::render::Renderer renderer(window.Handle(), window.Width(), window.Height(), validation);
        renderer.EnableUi();

        LogInfo("Initialized in {:.0f} ms: {}x{}, tearing {}", initTimer.ElapsedMs(), window.Width(), window.Height(),
                renderer.TearingSupported() ? "supported" : "not supported");

        const std::optional<uint32_t> smokeTestFrames = options.smokeTestFrames;
        if (smokeTestFrames) {
            LogInfo("Smoke test: rendering {} frames", *smokeTestFrames);
        }

        // Looks at the cube from above and to the side, so three faces are visible
        engine::scene::Camera camera;
        camera.position = {3.0f, 4.0f, -8.0f};
        engine::scene::LookAt(camera, {0.0f, 0.0f, 0.0f});
        camera.fovDegrees = 60.0f;
        const engine::scene::CameraController cameraController;

        engine::render::RenderList renderList; // this frame's movable objects

        // Destroyed before the renderer, which owns what the cache created
        engine::assets::AssetCache assets(renderer);
        engine::scene::Scene scene;

        // The textured grid (--count, --grid), as scene objects like everything else
        const engine::render::MeshHandle cubeMesh = renderer.CreateMesh(engine::assets::MakeCube(), L"Cube").front();
        // Without --texture, the sandbox's grid texture from data/ (the bench keeps the checkerboard)
        const std::filesystem::path gridTexturePath =
            options.texturePath.value_or(engine::core::ExecutableDirectory() / sandbox::kDefaultGridTexture);
        const sandbox::GridObjects grid =
            sandbox::AddGrid(scene, cubeMesh, engine::apps::CreateGridTexture(renderer, gridTexturePath),
                             options.gridMode.value_or(engine::apps::GridMode::Dynamic),
                             options.gridCount.value_or(engine::apps::kDefaultGridCount));
        // Static objects go into GPU memory once, so none may be added after this. Everything else is uploaded every
        // frame.
        std::vector<engine::render::StaticListHandle> staticLists;
        if (grid.mode == engine::apps::GridMode::Static) {
            engine::render::RenderList staticObjects;
            engine::scene::Extract(scene, staticObjects, engine::scene::Mobility::Static);
            staticLists.push_back(renderer.CreateStaticList(staticObjects, L"Grid/Instances"));
        }

        // Models in a row: --model, else all of assets/ (skipped in a smoke test: not in git)
        std::vector<std::filesystem::path> modelPaths;
        if (options.modelPath) {
            modelPaths.push_back(*options.modelPath);
        } else if (!smokeTestFrames) {
            modelPaths = sandbox::FindAssetModels();
        }
        const size_t modelsShown = sandbox::AddModels(scene, assets, modelPaths);
        // Where a model dropped on the sky goes: the end of the row
        float nextModelX = modelsShown > 0
                               ? (static_cast<float>(modelPaths.size() - 1) / 2 + 1) * sandbox::kModelSpacing
                               : sandbox::kModelSpacing;

        // Cubes the user can select and move, next to the models: a textured cube spinning at the origin (only
        // without models, which stand there) and three plain-coloured cubes around it. A material is its albedo
        // texture for now (Handles.h).
        const auto addCube = [&scene, cubeMesh](std::string name, engine::render::TextureHandle texture, float x,
                                                float z) {
            const engine::render::ModelPart part{
                .mesh = cubeMesh, .material = {.index = texture.index}, .bounds = engine::assets::kCubeBounds};
            DirectX::XMFLOAT4X4 world{};
            DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(x, 0.0f, z));
            return scene.AddObject(std::move(name), {&part, 1}, world, engine::scene::Mobility::Movable);
        };
        std::optional<engine::scene::ObjectId> spinningCube;
        if (modelsShown == 0) {
            spinningCube = addCube("Spinning cube", sandbox::CreateSpinningCubeTexture(renderer), 0.0f, 0.0f);
        }
        const engine::scene::ObjectId orangeCube =
            addCube("Orange cube", renderer.CreateSolidColorTexture(230, 120, 30, L"Orange"), -5.0f, 0.0f);
        addCube("Green cube", renderer.CreateSolidColorTexture(60, 170, 70, L"Green"), 5.0f, 0.0f);
        addCube("Blue cube", renderer.CreateSolidColorTexture(50, 110, 220, L"Blue"), 0.0f, 5.0f);
        // The smoke test selects a movable cube, so that the gizmo runs in every frame
        std::optional<engine::scene::ObjectId> selected;
        if (smokeTestFrames) {
            selected = orangeCube;
        }

        // Edited by the renderer window and the shortcuts I and V; the renderer takes a copy in BeginFrame
        engine::render::RendererSettings settings;

        // For the renderer window; the grid does not change while the sandbox runs
        const std::string sceneInfo =
            std::format("grid {} x {}, {}", grid.side, grid.side, engine::apps::ToString(grid.mode));

        // Game loop: handle all pending window messages, update, render
        engine::core::FrameTimer frameTimer;
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

            const engine::render::RendererSettings previousSettings = settings;
            if (appKeyboard) {
                if (input.WasKeyPressed(VK_ESCAPE)) {
                    window.Close();
                }
                if (input.WasKeyPressed('V')) {
                    settings.vsync = !settings.vsync;
                }
                if (input.WasKeyPressed('I')) {
                    settings.drawPath = (settings.drawPath == engine::render::DrawPath::Instanced)
                                            ? engine::render::DrawPath::NonInstanced
                                            : engine::render::DrawPath::Instanced;
                }
                if (input.WasKeyPressed(VK_F11) || (input.WasKeyPressed(VK_RETURN) && input.IsKeyDown(VK_MENU))) {
                    window.ToggleFullscreen();
                }
            }

            cameraController.Update(camera, input, frameTimer.DeltaSeconds(),
                                    {.mouse = appMouse, .keyboard = appKeyboard});

            // One view for the gizmo, picking and rendering. BeginFrame gives the swap chain the window's size, so
            // the window's aspect ratio is also the rendered one.
            const DirectX::XMFLOAT2 viewportPx{static_cast<float>(window.Width()), static_cast<float>(window.Height())};
            const engine::render::FrameView frameView =
                engine::apps::MakeFrameView(camera, viewportPx.x / viewportPx.y);
            if (spinningCube) {
                using namespace DirectX;
                // 90 degrees per second around (0, 1, 1), like the step-1 cube. fmod in double keeps the angle precise
                // however long the program runs.
                const float angle =
                    XMConvertToRadians(static_cast<float>(std::fmod(frameTimer.TotalSeconds() * 90.0, 360.0)));
                const XMFLOAT3 position = ObjectPosition(scene.Object(*spinningCube));
                XMFLOAT4X4 world{};
                // Row vectors: spin first, then move, so it spins around its own (moved) centre
                XMStoreFloat4x4(&world, XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f), angle) *
                                            XMMatrixTranslation(position.x, position.y, position.z));
                scene.SetObjectWorld(*spinningCube, world);
            }

            // The gizmo on the selected object, unless it is static; it moves the object while a handle is dragged
            if (selected && scene.Object(*selected).mobility == engine::scene::Mobility::Movable) {
                const DirectX::XMFLOAT3 position = ObjectPosition(scene.Object(*selected));
                // The position only: the gizmo's axes are the world's (ImGuizmo::WORLD), however the object is
                // turned or scaled
                DirectX::XMFLOAT4X4 gizmoWorld{};
                DirectX::XMStoreFloat4x4(&gizmoWorld, DirectX::XMMatrixTranslation(position.x, position.y, position.z));
                if (ui.TranslateGizmo(frameView.view, frameView.projection, gizmoWorld)) {
                    MoveObject(scene, *selected, {gizmoWorld._41, gizmoWorld._42, gizmoWorld._43});
                }
            }
            // A left click on the scene selects the object under the cursor, or clears the selection. After the
            // gizmo, so that a click on one of its handles stays the gizmo's.
            if (appMouse && !ui.GizmoWantsMouse() && input.WasKeyPressed(VK_LBUTTON)) {
                const engine::core::Int2 mouse = input.MousePosition();
                const engine::scene::Ray ray = engine::scene::MakePickRay(
                    camera, {static_cast<float>(mouse.x) + 0.5f, static_cast<float>(mouse.y) + 0.5f}, viewportPx);
                selected = engine::scene::Pick(scene, ray);
                LogInfo("Selected: {}", selected ? std::string_view(scene.Object(*selected).name) : "nothing");
            }

            ui.ShowRendererWindow({.fps = frameTimer.Fps(),
                                   .objectCount = scene.EntityCount(),
                                   .scene = sceneInfo,
                                   .cameraPosition = camera.position},
                                  settings);
            if (selected) {
                const engine::scene::SceneObject &object = scene.Object(*selected);
                DirectX::XMFLOAT3 position = ObjectPosition(object);
                if (ui.ShowSelectionWindow(object.name, position,
                                           object.mobility == engine::scene::Mobility::Movable)) {
                    MoveObject(scene, *selected, position);
                }
            } else {
                ui.ShowEmptySelectionWindow();
            }
            ImDrawData *const uiDrawData = ui.Render();
            LogSettingsChanges(previousSettings, settings);

            // Everything above has read this frame's input; clear the
            // per-frame values (pressed, deltas).
            input.EndFrame();

            // Between frames: EndFrame submits their uploads before the frame
            for (const DroppedFile &drop : std::exchange(droppedFiles, {})) {
                const engine::scene::Ray ray = engine::scene::MakePickRay(
                    camera, {static_cast<float>(drop.x) + 0.5f, static_cast<float>(drop.y) + 0.5f}, viewportPx);
                sandbox::LoadDroppedFile(drop.path,
                                         engine::scene::IntersectHorizontalPlane(ray, engine::apps::kGridTop), renderer,
                                         assets, scene, nextModelX, grid);
            }

            renderList.Clear();
            engine::scene::Extract(scene, renderList, engine::scene::Mobility::Movable);

            try {
                renderer.BeginFrame(window.Width(), window.Height(), settings);
                renderer.Render(frameView, renderList, staticLists);
                renderer.RenderUi(uiDrawData); // on top of the scene
                renderer.EndFrame();
            } catch (const engine::core::HResultError &) {
                // Reset, ResizeBuffers, Present, Signal and others fail with
                // DXGI_ERROR_DEVICE_REMOVED once the GPU is lost
                renderer.LogDeviceRemovedInfo();
                throw;
            }
            ++framesRendered;

            if (smokeTestFrames) {
                const uint32_t frames = *smokeTestFrames;
                // Two fullscreen toggles exercise the resize path: flush, ResizeBuffers, new RTVs and
                // the deferred release of the old depth buffer
                if (frames >= 3 && (framesRendered == frames / 3 || framesRendered == 2 * frames / 3)) {
                    window.ToggleFullscreen();
                }
                // The second half runs on the other draw path, so both stay covered by the smoke test
                if (framesRendered == frames / 2) {
                    settings.drawPath = engine::render::DrawPath::NonInstanced;
                    LogInfo("Smoke test: draw path non-instanced from frame {}", framesRendered + 1);
                }
                if (framesRendered == frames) {
                    window.Close();
                }
            }
        }

        return {.exitCode = window.ExitCode(), .framesRendered = framesRendered};
    }
} // namespace

int WINAPI wWinMain(HINSTANCE /*hInst*/, HINSTANCE /*prevInst*/, PWSTR /*pCmdLine*/, int /*nCmdShow*/) {
    // Known before anything that can fail later; the catch below needs it
    bool smokeTest = false;
    try {
        const std::expected<SandboxOptions, std::string> options = ParseSandboxOptions(engine::core::CommandLineArgs());
        if (!options) {
            // No dialog: a wrong command line usually comes from a script
            LogError("{}", options.error());
            LogInfo("{}", kSandboxUsage);
            return engine::apps::kExitBadCommandLine;
        }
        smokeTest = options->smokeTestFrames.has_value();
        if (smokeTest) {
            // A failed ImGui or D3D12MA assert must end the run, not wait at a dialog until ctest's timeout
            engine::apps::RouteCrtAssertsToStderr();
        }

        // For the whole run: image files are decoded through WIC, which needs COM
        const engine::core::ComScope com;

        // Outlives the renderer: its device writes the live-object count when it is destroyed
        engine::rhi::ValidationStats validation;
        const engine::apps::RunResult run = RunSandbox(*options, validation);
        engine::apps::LogValidationSummary(validation);

        const int exitCode =
            smokeTest ? engine::apps::SmokeTestExitCode(*options->smokeTestFrames, run, validation) : run.exitCode;
        LogInfo("Exiting with code {}", exitCode);
        return exitCode;
    } catch (std::exception &e) {
        LogError("{}", e.what());
        // A dialog would block an unattended run until someone closes it
        if (!smokeTest) {
            MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
        }
        return -1;
    }
}
