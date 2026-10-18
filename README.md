# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, the successor of
[DX11 engine](https://github.com/bilyi1andrii/graphics_hw_bilyi).

**Status:** step 2 of the plan (camera, textures, models, instancing) is in progress. The sandbox shows a cube that
you can fly around with a camera. In place are the build system (CMake presets, vcpkg, the D3D12 Agility SDK, and HLSL
compiled by DXC at build time), the platform layer in `engine/core` (window, input, timing, logging, error handling),
the D3D12 wrapper in `engine/rhi` (device with debug layer and feature checks, queues, frames in flight, descriptors,
swap chain, memory through D3D12MA, per-frame upload memory, a bindless descriptor heap), one global root signature
defined in HLSL, a fly camera in `engine/scene`, the `Renderer` in `engine/render`, and an automated smoke test.
Next: instancing, textures and models.

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

Presets: `x64-debug`, `x64-release`, `x64-relwithdebinfo` (benchmarks).

The build uses MSVC with the Ninja generator, so CMake must run inside the MSVC environment, where `cl.exe`,
CMake and Ninja are on `PATH`. From a terminal, use a **Developer PowerShell for VS** (or run `vcvars64.bat` first):

```powershell
cmake --preset x64-debug              # configure (first run also installs vcpkg packages)
cmake --build --preset x64-debug      # build
.\build\x64-debug\bin\sandbox.exe     # run
ctest --preset x64-debug              # smoke test
```

An IDE that understands `CMakePresets.json` can open the repo folder directly. Select a configure preset, build, and
run the `sandbox` target. The IDE must either set up the MSVC environment itself or be started from a Developer
PowerShell. Debug the exe with the Visual Studio debugger, or another debugger that reads MSVC PDB files.

### Controls

| Key | Action |
|---|---|
| Esc | Quit |
| V | Toggle vsync (the FPS is shown in the title bar) |
| F11 or Alt+Enter | Toggle borderless fullscreen |
| Right mouse button + mouse | Look around |
| W / A / S / D | Fly forward / left / back / right |
| Space / Left Shift | Fly up / down |

### Log output

The app logs to stderr and to the debugger output. Debug and RelWithDebInfo builds are console programs, so the log
appears in the terminal (or the IDE's terminal) that started them. A Debug build started from Explorer therefore opens a
console window next to the game window. Release builds have no console; capture their log with `sandbox.exe 2> log.txt`.

### Smoke test

`ctest --preset x64-debug` runs `sandbox.exe --smoke-test 120`: it renders 120 frames, toggles fullscreen twice
(the screen flashes for about a second) and exits on its own. The test fails if the D3D12 or DXGI debug layer reported
an error, if objects are still alive at shutdown, if fewer than 120 frames were rendered, or if the app crashes or
hangs (ctest stops it after 30 s). On failure ctest prints the log; the last lines say why. Run it after every change,
before committing.

The exe can also be started directly with `--smoke-test N`. Exit codes:

| Code | Meaning |
|---|---|
| 0 | Passed |
| 1 | Failed (the log says why) |
| 2 | Bad command line |
| 3 | Failed assert |
| -1 | Exception |

Only Debug builds have the debug layer. In Release the test only checks that the app renders and shuts down.

### Output layout

```
build/x64-debug/bin/
  sandbox.exe
  D3D12MAd.dll  D3D12 Memory Allocator (D3D12MA.dll in Release), copied by vcpkg
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
apps/sandbox   test application (entry point, --smoke-test)
cmake/         CMake helper modules (warnings, Agility SDK copy)
engine/core    platform layer: Window, Input, Timer, Log, ThrowIfFailed, Assert, File, CommandLine (no D3D12)
engine/rhi     thin D3D12 wrapper: Device, CommandQueue, FrameContext, CommandContext, SwapChain, descriptors,
               Allocator (D3D12MA), Buffer, DepthBuffer, UploadRing (per-frame upload memory),
               BindlessHeap (the shader-visible descriptor heap; shaders read resources by index)
engine/scene   Camera and fly-camera controls (no D3D12)
engine/render  Renderer: owns the frame loop's GPU objects and records each frame
shaders/       HLSL sources; vs_/ps_/cs_ prefix selects the stage (SM 6.6), rs_ compiles a root signature
shaders/common shared with C++: root_signature.hlsli (the global root signature), shared_types.hlsli (constants)
```

More engine modules (`engine/assets`, `engine/ui`, …) and the `bench` app are added in the next steps.
