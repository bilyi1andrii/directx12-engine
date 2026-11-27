# DirectX 12 Engine

A real-time renderer in C++23 on Direct3D 12, and a sandbox for rendering experiments. It is the successor of our
[Andrii's](https://github.com/bilyi1andrii/graphics_hw_bilyi), [Ira's](https://github.com/hhafiya/graphics-hw-kyrylova) engines, which it must match in features and beat in speed
and scaling.

## Authors

- Andrii ([bilyi1andrii](https://github.com/bilyi1andrii))
- Ira Kyrylova ([hhafiya](https://github.com/hhafiya))

## Structure

```
engine/core        window, input, timing, logging, error handling
engine/rhi         thin Direct3D 12 layer: device, queues, frames in flight, descriptors, memory, uploads, pipelines
engine/render      the renderer and the ImGui backend
engine/renderdata  plain CPU data the renderer draws from: render lists, handles, settings, mesh and texture data
engine/scene       objects, camera, camera paths, picking
engine/assets      model and texture loading, asset cache
engine/ui          debug UI: panels and gizmo
shaders/           HLSL, compiled by DXC during the build
apps/sandbox       interactive test application
apps/bench         benchmark
apps/common        code shared by both apps
data/              small runtime files the apps always need
tests/             unit tests of the CPU code
```

Only `engine/rhi` and `engine/render` work with Direct3D 12 directly. Everything above them passes handles and plain
data.

## Features

- Direct3D 12 with the Agility SDK, Shader Model 6.6 bindless resources and one global root signature defined in HLSL
- Two frames in flight, GPU memory through D3D12MA, and deferred release of everything the GPU may still be using
- Blinn-Phong lighting: a directional light plus point and spot lights that can be selected, moved and edited
- Soft shadows from the directional light (a shadow map fitted to the scene, 3×3 filtered comparisons)
- Automatic batching by mesh and material, drawn either instanced or with one draw call per object
- Moving objects uploaded every frame, static objects uploaded once to GPU memory
- Models from glTF, OBJ and FBX files (Assimp), textures with generated mips (DirectXTex)
- Fly camera, mouse selection, a move/rotate/scale gizmo and an ImGui overlay with statistics and settings
- GPU time of every frame and every render pass, from timestamp queries, shown live and written by the benchmark
- Loading models and images by dropping them on the window
- The sandbox's scene saved to JSON on exit and restored at the next start
- A benchmark with a scripted camera that writes per-frame CPU and GPU timings to CSV
- Debug layer, GPU-based validation and DRED in Debug builds, plus smoke tests, unit tests and an AddressSanitizer build

There is no tonemapping yet, so very bright areas clip to white.

## Showcase

### Shadows

Shadow map, 3×3 filtering.

<table>
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/12e55d66-0893-42da-a249-fe86fc70dd8b" width="100%" alt="Shadows in the Assisi courtyard"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/cc38ce9e-e00e-46a0-85d9-0aa16c345ce2" width="100%" alt="Shadows on the Assisi square"></td>
  </tr>
</table>

### Point and spot lights

Blinn-Phong.

<table>
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/ed19215a-1fcb-47b2-8e7a-3692467c874c" width="100%" alt="Spot lights in an Assisi alley"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/8bd4870b-bf2f-49b3-ab39-4979358b8c9b" width="100%" alt="Spot light behind the gate"></td>
  </tr>
</table>

### Transparency

<table>
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/4ab0a65b-31f2-4fdc-9888-58e71b16633b" width="100%" alt="Ducks among lily pads"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/a1946f65-1cb8-4aca-8d58-46d8d1bc5b26" width="100%" alt="Transparent pond water"></td>
  </tr>
</table>

### More scenes

<table>
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/0c40c555-3c63-4bc0-9608-61e0bc89547b" width="100%" alt="Kyoto street"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/2f7942d9-0369-4e77-ae7b-84b35afd83cc" width="100%" alt="Kyoto torii"></td>
  </tr>
</table>

## Models
 
Models from Sketchfab, not included in the repository.
 
| Model | Author | License |
|---|---|---|
| [Tanabata evening - Kyoto inspired city scene](https://sketchfab.com/3d-models/tanabata-evening-kyoto-inspired-city-scene-04dc9402b74d43ef86c4795311c0e4bb) | [Mathias Tossens](https://sketchfab.com/mathiastossens) | [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) |
| [Assisi - City Scene Assignment](https://sketchfab.com/3d-models/assisi-city-scene-assignment-8b3dc82bc269464e8a9632b8094d538d) | [Nioma van der Steen](https://sketchfab.com/fromthestone) | [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) |
| [Duck-Duck](https://sketchfab.com/3d-models/duck-duck-98604d4f895b4fc79d880dd427bf3a01) | [td991](https://sketchfab.com/td991) | [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/) |

## How to run

You need:

- Windows 10 or 11, x64, and a GPU with resource binding tier 3 and Shader Model 6.6
- Visual Studio 2022 or 2026 with the **Desktop development with C++** workload (MSVC, Windows SDK, CMake, Ninja)
- vcpkg, with the `VCPKG_ROOT` environment variable pointing to it (Visual Studio's copy is in `<VS install>\VC\vcpkg`)

Run these from a **Developer PowerShell for VS**, or any shell where `vcvars64.bat` has been run. Dependencies are
installed on the first configure, which takes several minutes.

```powershell
cmake --preset x64-release
cmake --build --preset x64-release
.\build\x64-release\bin\sandbox.exe
```

Any editor or IDE that reads `CMakePresets.json` works too, as long as it runs inside the MSVC environment.

| Preset | Use |
|---|---|
| `x64-debug` | Development: debug layer on, log in the terminal |
| `x64-release` | Normal use and benchmark numbers; no console |
| `x64-relwithdebinfo` | Profiling: optimized, with symbols and a console |
| `x64-asan`, `x64-analyze` | Checks: AddressSanitizer, MSVC static analysis |

To run the unit and smoke tests, use `ctest --preset x64-debug` after a build. The screen flashes briefly while the
smoke tests toggle fullscreen.

### Sandbox

The sandbox shows a grid of textured cubes, a few cubes you can select and move, and the models from the `assets/` folder in a row (one folder per model, for example `assets/fantasy_town/scene.gltf`). Without any models, there is a
spinning cube in the middle. While it runs, you can drop a model or an image on the window: a model stands where
you drop it, and an image becomes the grid's texture (dynamic grid only).

Two point lights and two spot lights shine on the scene, each shown as a small sphere in its colour. Click a sphere
to select its light, move it with the gizmo, and turn a spot light with the gizmo's rotate mode. The Lights window in
the bottom-right corner edits the ambient, specular and directional light, and the selected light's colour, strength,
fade-out and cone. Lights are not saved.

The Renderer window in the top-left corner shows the frame rate, the CPU time spent building and recording the frame,
and the GPU time of the frame and of each render pass, averaged over one second.

The scene is saved to `saves/sandbox.json` when the window closes and restored at the next start.

| Option | Effect |
|---|---|
| `--model PATH` | Show this model instead of everything in `assets/`; the scene is neither loaded nor saved |
| `--texture PATH` | Use this image on the grid |
| `--count N` | Cubes in the grid, rounded up to a square (default: the saved scene's, else 400) |
| `--grid static\|dynamic` | Upload the grid once, or every frame (default: the saved scene's, else `dynamic`) |
| `--smoke-test N` | Render N frames, then exit with the test result; the scene is neither loaded nor saved |

| Control | Action |
|---|---|
| Right mouse button + mouse | Look around |
| Right mouse button + W / A / S / D | Fly forward / left / back / right |
| Right mouse button + Space / Left Shift | Fly up / down |
| Right mouse button + mouse wheel | Fly faster / slower |
| Left mouse button | Select an object or a light's sphere; a click on empty space clears the selection |
| W / E / R | Gizmo mode for the selected object: move / rotate / scale |
| I | Switch between the instanced and the non-instanced draw path |
| V | Toggle vsync |
| F11 or Alt+Enter | Toggle borderless fullscreen |
| Esc | Quit |

### Benchmark

`bench.exe` measures how the engine scales with the number of objects. It draws a square grid of copies of one model
while a scripted camera flies around it, lit by a fixed set of lights (one directional, two point, two spot), in
borderless fullscreen with vsync off, and closes by itself. Use the
`x64-release` build: the others are too slow for their numbers to mean anything.

```powershell
.\build\x64-release\bin\bench.exe --grid static --count 50000
.\build\x64-release\bin\bench.exe --model assets\fantasy_town\scene.gltf --count 100
```

| Option | Effect |
|---|---|
| `--model FILE` | Draw copies of this model, needs `--count` (default: a cube) |
| `--count N` | Copies in the grid, rounded up to a square (default 50000 for the cube) |
| `--grid static\|dynamic` | Upload the grid once, or every frame (default `dynamic`) |
| `--instanced 1\|0` | One draw per batch (default), or one draw per object |
| `--frames N`, `--warmup N` | Measured frames (default 1000) and frames before measuring (default 200) |
| `--out DIR` | Results folder (default `results`) |

Each run writes to `<out>/dx12/<name>/<date-time>/`, where `<name>` is `cube` or the model's folder and file name.
`frames.csv` holds the CPU times of every frame, the GPU time of the frame and of each render pass
(`gpu_total_ms`, `gpu_opaque_ms`, ...), and `run.txt` holds the settings, the size of the scene, the lights, the GPU,
the driver and the medians. Pressing Esc stops the run and writes nothing.

## Next Steps

- HDR and tonemapping
- PBR materials
- Performance comparison with the DX11 engine
- Extensions: skybox, normal mapping, deferred rendering, stylized post-processing

