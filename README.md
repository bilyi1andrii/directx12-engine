# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, the successor of
[DX11 engine](https://github.com/bilyi1andrii/graphics_hw_bilyi).

**Status:** step 2 of the plan (camera, textures, models, instancing) is in progress. The sandbox shows a grid of
400 textured cubes and one spinning cube that you can fly around with a camera, drawn either instanced (one draw call
per mesh and material) or with one draw call per object. In place are the build system (CMake presets, vcpkg, the
D3D12 Agility SDK, and HLSL compiled by DXC at build time), the platform layer in `engine/core` (window, input, timing,
logging, error handling), asset loading in `engine/assets` (models through Assimp; textures through DirectXTex, mips
generated on the CPU), the D3D12 wrapper in `engine/rhi` (device with debug layer and feature checks, queues, frames
in flight, descriptors, swap chain, memory through D3D12MA, per-frame and static uploads, textures, a bindless
descriptor heap, a pipeline cache), one global root signature defined in HLSL, a scene of entities and a fly camera
in `engine/scene`, the `Renderer` in `engine/render`, the plain CPU data it draws from in `engine/renderdata` (the
`RenderList` of objects batched by mesh and material, plus the texture, mesh, material and model types), per-frame
instance data in page-sized chunks plus static lists uploaded once to GPU memory (`--count 50000` draws a 50k-cube
grid), a benchmark app that writes per-frame CPU times to CSV, unit tests and automated smoke tests. Models from
files (glTF, OBJ, FBX, ...) are shown on the cube grid. Next: drawing Sponza and ImGui.

## Requirements

- Windows 10/11, x64
- Visual Studio 2022 or 2026 with the **Desktop development with C++** workload (MSVC, Windows SDK, CMake, Ninja)
- vcpkg (the copy bundled with Visual Studio works: `<VS install>\VC\vcpkg`)
- Any editor or IDE that supports CMake presets, or just a terminal
- Optional: PIX for GPU debugging

## One-time setup

Point the `VCPKG_ROOT` environment variable at your vcpkg folder, for example:

```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg", "User")
```

Restart your IDE or terminal afterwards so it picks up the variable.

The dependencies (`vcpkg.json`) are installed automatically on the first configure, into
`build/<preset>/vcpkg_installed/`.

## Build and run

Presets: `x64-debug`, `x64-release` (also for benchmarks), `x64-relwithdebinfo` (for profiling), and two for extra
checks: `x64-asan` (Debug with AddressSanitizer) and `x64-analyze` (Debug with the compiler's static analysis,
`/analyze`).

The build uses MSVC with the Ninja generator, so CMake must run inside the MSVC environment, where `cl.exe`,
CMake and Ninja are on `PATH`. From a terminal, use a **Developer PowerShell for VS** (or run `vcvars64.bat` first):

```powershell
cmake --preset x64-debug              # configure (first run also installs vcpkg packages)
cmake --build --preset x64-debug      # build
.\build\x64-debug\bin\sandbox.exe     # run
ctest --preset x64-debug              # smoke tests and unit tests
```

Command-line options of `sandbox.exe`:

| Option | Effect |
|---|---|
| `--texture PATH` | Show an image file (`.png`, `.jpg`, `.dds`, `.tga`, ...) on the cube grid instead of `data/textures/Endstone.jpg` |
| `--count N` | Number of cubes in the grid, rounded up to a square (default 400, a 20×20 grid); e.g. `--count 50000` gives 224×224 = 50,176 cubes for a stress test |
| `--grid static\|dynamic` | Upload the grid every frame, like moving objects (`dynamic`, the default), or once (`static`) |
| `--model PATH` | Show this model file (`.gltf`, `.glb`, `.obj`, `.fbx`, ...) on the grid. Without it, every model in the repo's `assets/` folder is shown in a row (except in a smoke test); without any model, a spinning cube |
| `--smoke-test N` | Render N frames and exit (see [Smoke test and unit tests](#smoke-test-and-unit-tests)) |

Models are not committed: put them in `assets/` at the repository root (gitignored), each in its own folder with its
textures, e.g. `assets/fantasy_town/scene.gltf`. Credit the authors as their licenses require. Small files the apps
always need (the grid's texture) are committed in `data/`; the build copies them to `build/<preset>/bin/data/`.

While the sandbox runs, drag files from Explorer onto its window: a model (`.gltf`, `.glb`, `.obj`, `.fbx`) stands
where you drop it, on the height of the grid's top (dropped on the sky, it joins the row of models); any other file is
tried as an image for the grid. The window waits while a file loads (a large model takes seconds). Windows does not
allow dropping from Explorer onto a program that runs as administrator.

An IDE that understands `CMakePresets.json` can open the repo folder directly. Select a configure preset, build, and
run the `sandbox` target. The IDE must either set up the MSVC environment itself or be started from a Developer
PowerShell. Debug the exe with the Visual Studio debugger, or another debugger that reads MSVC PDB files.

### Controls

| Key | Action |
|---|---|
| Esc | Quit |
| V | Toggle vsync (the FPS is shown in the title bar) |
| I | Toggle the draw path: instanced or one draw per object (shown in the title bar; the image stays the same) |
| F11 or Alt+Enter | Toggle borderless fullscreen |
| Right mouse button + mouse | Look around |
| W / A / S / D | Fly forward / left / back / right |
| Space / Left Shift | Fly up / down |

### Log output

The app logs to stderr and to the debugger output. Debug and RelWithDebInfo builds are console programs, so the log
appears in the terminal (or the IDE's terminal) that started them. A Debug build started from Explorer therefore opens a
console window next to the game window. Release builds have no console; capture their log with `sandbox.exe 2> log.txt`.

### Smoke test and unit tests

`ctest --preset x64-debug` runs the smoke test and the unit tests. The unit tests (`tests/`, GoogleTest) check CPU
code such as file reading and texture loading; run them all at once with `build\x64-debug\bin\engine_tests.exe`.

The smoke test runs `sandbox.exe --smoke-test 120`: it renders 120 frames, toggles fullscreen twice
(the screen flashes for about a second), switches to the non-instanced draw path halfway, and exits on its own. The
test fails if the D3D12 or DXGI debug layer reported an error, if objects are still alive at shutdown, if fewer than
120 frames were rendered, or if the app crashes or hangs (ctest stops it after 30 s). On failure ctest prints the log;
the last lines say why. Run it after every change, before committing.

A second smoke test, `bench_smoke`, runs a very short [benchmark](#benchmark) with the same checks.

The exe can also be started directly with `--smoke-test N`. Exit codes:

| Code | Meaning |
|---|---|
| 0 | Passed |
| 1 | Failed (the log says why) |
| 2 | Bad command line |
| 3 | Failed assert |
| -1 | Exception |

Only Debug builds have the debug layer. In Release the test only checks that the app renders and shuts down.

### Benchmark

`bench.exe` measures how fast the engine renders a fixed scene, so changes (and later the DX11 engine) can be
compared on the same workload. It flies a camera around the cube grid for a few seconds in fullscreen and closes by
itself. The CPU time of every frame goes to `results/dx12/<scene>/<date-time>/frames.csv`, the settings, GPU and
driver to `run.txt` next to it.

Run it from the `x64-release` build: the other builds are slower, so their numbers can't be compared.

### Output layout

```
build/x64-debug/bin/
  sandbox.exe
  bench.exe     benchmark
  engine_tests.exe  unit tests
  D3D12MAd.dll  D3D12 Memory Allocator (D3D12MA.dll in Release), copied by vcpkg
  DirectXTex.dll, gtest.dll  texture loading and GoogleTest, copied by vcpkg
  clang_rt.asan_dynamic-x86_64.dll  AddressSanitizer runtime (x64-asan only)
  D3D12/        D3D12Core.dll (+ d3d12SDKLayers.dll in Debug): Agility SDK runtime
  shaders/      *.cso compiled by DXC: shaders (vs_/ps_/cs_) and the root signature (rs_global.cso)
```

## Troubleshooting

- **`cl.exe is not a full path and was not found in the PATH`:** CMake ran without the MSVC environment.
  Configure from a Developer PowerShell, or start your IDE from one.
- **`.../scripts/buildsystems/vcpkg.cmake` not found:** `VCPKG_ROOT` is not set, or the IDE was started before you set
  it.
- **A new header, source file or vcpkg package is not found after pulling changes:** `CMakeLists.txt` or `vcpkg.json`
  changed; configure again (`cmake --preset x64-debug`, or the IDE's configure command). Code completion in the IDE
  may also need that before it finds new include paths.
- **Wrong `cmake`/`ninja` picked up** (another toolchain earlier on `PATH`): `where cmake ninja` should list Visual
  Studio's `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` and `...\CMake\Ninja` first.

## Repository layout

```
apps/sandbox   test application (entry point, --smoke-test, --count, --grid, --texture, --model; models and
               dropped files in SandboxScene)
apps/bench     benchmark: renders a fixed scene and writes per-frame times to CSV
apps/common    code both apps share (the cube-grid scene, option parsing, end-of-run checks)
cmake/         CMake helper modules (warnings, Agility SDK and ASan runtime copy)
data/          small runtime files committed to git (textures/Endstone.jpg, the grid's texture); copied to bin/data/
engine/core    platform layer: Window, Input, Timer, Log, ThrowIfFailed, Assert, Error, File, CommandLine,
               ComScope (no D3D12)
engine/assets  ModelImporter: reads model files with Assimp into geometry, node placements and materials;
               TextureLoader: loads images (files or bytes inside a model) with DirectXTex and generates mips;
               AssetCache: loads each model and texture once and uploads it through the renderer's GpuUploader
               interface; Primitives: meshes made in code (the cube) (no D3D12)
engine/rhi     thin D3D12 wrapper: Device, CommandQueue, FrameContext, CommandContext, SwapChain, descriptors,
               Allocator (D3D12MA), Buffer, DepthBuffer, Texture, UploadRing (per-frame upload memory),
               UploadContext (static uploads), BindlessHeap (the shader-visible descriptor heap; shaders read
               resources by index), PipelineCache (PSOs built from a description and cached; the global
               root signature)
engine/scene   Scene (entities with a transform and a mesh + material; Instantiate places a model, Extract fills
               the RenderList), Camera, fly-camera controls, scripted camera paths and Picking (the ray under a
               screen point) (no D3D12)
engine/renderdata
               plain CPU data the renderer draws from (no D3D12): RenderList (the frame's objects batched by mesh
               and material), InstanceChunks (how instance data is split into upload pages), RendererSettings
               (e.g. the draw path), Handles (MeshHandle, MaterialHandle, TextureHandle, StaticListHandle),
               TextureData, Vertex, MeshData, MaterialDesc, ModelData for model loading, Model (a loaded model's
               parts) and GpuUploader (the interface asset loading creates GPU data through)
engine/render  Renderer: owns the frame loop's GPU objects and records each frame; draws static lists (uploaded
               once) and the frame's RenderList (uploaded every frame); CreateMesh uploads a model's geometry
               into one vertex and one index buffer and returns a MeshHandle per submesh
shaders/       HLSL sources; vs_/ps_/cs_ prefix selects the stage (SM 6.6), rs_ compiles a root signature
shaders/common shared with C++: root_signature.hlsli (the global root signature), shared_types.hlsli (constants,
               per-draw root constants, instance data); vs_output.hlsli (vertex to pixel shader data)
tests/         unit tests of the CPU code (GoogleTest; no GPU needed)
```

More engine modules (`engine/ui`, …) are added in the next steps.
