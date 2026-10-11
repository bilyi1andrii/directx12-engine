# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, the successor of
[DX11 engine](https://github.com/bilyi1andrii/graphics_hw_bilyi).

**Status:** step 1 of the plan, so far a clear-screen window. The build system is in place
(CMake presets, vcpkg, the D3D12 Agility SDK, and HLSL compiled by DXC at build time), and so is the platform
layer in `engine/core` (window, input, timing, logging, error handling).

## Requirements

- Windows 10/11, x64
- Visual Studio 2022 or 2026 with the **Desktop development with C++** workload (MSVC, Windows SDK, CMake, Ninja)
- vcpkg (the copy bundled with Visual Studio works: `<VS install>\VC\vcpkg`)
- Optional: CLion 2024.3 or newer, PIX for GPU debugging

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

### CLion

1. Open the repo folder. CLion imports the presets from `CMakePresets.json` as CMake profiles.
   Enable `x64-debug` in **Settings → Build, Execution, Deployment → CMake**.
2. The presets tell CLion to use a toolchain named **Visual Studio**. It must exist in **Settings → … → Toolchains**,
   with Architecture **amd64** and debugger **Bundled LLDB**.
3. Build and run the **sandbox** configuration.

### Command line

Use a **Developer PowerShell for VS**, so that `cl.exe`, CMake and Ninja are on `PATH`:

```powershell
cmake --preset x64-debug              # configure (first run also installs vcpkg packages)
cmake --build --preset x64-debug      # build
.\build\x64-debug\bin\sandbox.exe     # run
```

### Controls

| Key | Action |
|---|---|
| Esc | Quit |
| V | Toggle vsync (the FPS is shown in the title bar) |
| F11 or Alt+Enter | Toggle borderless fullscreen |

### Log output

The app logs to stderr and to the debugger output. Debug and RelWithDebInfo builds are console programs, so the log
appears in CLion's Run window or in the terminal. A Debug build started from Explorer therefore opens a console window
next to the game window. Release builds have no console; capture their log with `sandbox.exe 2> log.txt`.

### Output layout

```
build/x64-debug/bin/
  sandbox.exe
  D3D12/      D3D12Core.dll (+ d3d12SDKLayers.dll in Debug): Agility SDK runtime
  shaders/    *.cso compiled by DXC
```

## Troubleshooting

- **`cl.exe is not a full path and was not found in the PATH`:** CMake ran without the MSVC environment.
  In CLion, check that the toolchain is named exactly `Visual Studio`. On the command line, use the Developer
  PowerShell.
- **`.../scripts/buildsystems/vcpkg.cmake` not found:** `VCPKG_ROOT` is not set, or the IDE was started before you set
  it.
- **Wrong `cmake`/`ninja` picked up** (e.g. from MSYS2 or MinGW on `PATH`): put Visual Studio's
  `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` and `...\CMake\Ninja` first on `PATH`.

## Repository layout

```
apps/sandbox   test application (entry point)
cmake/         CMake helper modules (warnings, Agility SDK copy)
engine/core    platform layer: Window, Input, Timer, Log, ThrowIfFailed, Assert (no D3D12)
shaders/       HLSL sources; vs_/ps_/cs_ prefix selects the stage (SM 6.6)
```

More engine modules (`engine/rhi`, `engine/render`, …) are added in the next steps.
