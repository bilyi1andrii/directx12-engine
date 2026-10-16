# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, the successor of
[DX11 engine](https://github.com/bilyi1andrii/graphics_hw_bilyi).

**Status:** step 1 of the plan, so far a clear-screen window. The build system is in place
(CMake presets, vcpkg, the D3D12 Agility SDK, and HLSL compiled by DXC at build time), and so is the platform
layer in `engine/core` (window, input, timing, logging, error handling). The D3D12 wrapper in `engine/rhi`
is in progress (device with debug layer and feature checks, command queue with fence).

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
apps/sandbox   test application (entry point)
cmake/         CMake helper modules (warnings, Agility SDK copy)
engine/core    platform layer: Window, Input, Timer, Log, ThrowIfFailed, Assert (no D3D12)
engine/rhi     thin D3D12 wrapper: Device, CommandQueue (more to come in step 1)
shaders/       HLSL sources; vs_/ps_/cs_ prefix selects the stage (SM 6.6)
```

More engine modules (`engine/render`, `engine/scene`, …) are added in the next steps.
