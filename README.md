# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, the successor of
[DX11 engine](https://github.com/bilyi1andrii/graphics_hw_bilyi).

**Status:** step 1 of the plan is complete: a window with a spinning cube. In place are the build system
(CMake presets, vcpkg, the D3D12 Agility SDK, and HLSL compiled by DXC at build time), the platform layer in
`engine/core` (window, input, timing, logging, error handling), the D3D12 wrapper in `engine/rhi` (device with debug
layer and feature checks, queues, frames in flight, descriptors, swap chain, buffers), the `Renderer` in
`engine/render`, and an automated smoke test. Next: step 2 (camera, textures, models, instancing).

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
  D3D12/      D3D12Core.dll (+ d3d12SDKLayers.dll in Debug): Agility SDK runtime
  shaders/    *.cso compiled by DXC
```

## Troubleshooting

- **`cl.exe is not a full path and was not found in the PATH`:** CMake ran without the MSVC environment.
  Configure from a Developer PowerShell, or start your IDE from one.
- **`.../scripts/buildsystems/vcpkg.cmake` not found:** `VCPKG_ROOT` is not set, or the IDE was started before you set
  it.
- **Wrong `cmake`/`ninja` picked up** (another toolchain earlier on `PATH`): `where cmake ninja` should list Visual
  Studio's `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` and `...\CMake\Ninja` first.

## Repository layout

```
apps/sandbox   test application (entry point, --smoke-test)
cmake/         CMake helper modules (warnings, Agility SDK copy)
engine/core    platform layer: Window, Input, Timer, Log, ThrowIfFailed, Assert, File, CommandLine (no D3D12)
engine/rhi     thin D3D12 wrapper: Device, CommandQueue, FrameContext, CommandContext, SwapChain, descriptors, buffers
engine/render  Renderer: owns the frame loop's GPU objects and records each frame
shaders/       HLSL sources; vs_/ps_/cs_ prefix selects the stage (SM 6.6)
```

More engine modules (`engine/scene`, `engine/assets`, …) are added in the next steps.
