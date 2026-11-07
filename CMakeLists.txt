cmake_minimum_required(VERSION 3.24)
project(D3D12_Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every executable lands in build/<preset>/bin, next to D3D12/ (Agility runtime), shaders/ and data/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Tests run with `ctest --preset <preset>` after a build. Must be enabled in the top-level
# CMakeLists.txt, before the subdirectories that add tests.
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(EngineTargets)

# Packages come from vcpkg.json; the toolchain file is set by CMakePresets.json.
find_package(directx-headers CONFIG REQUIRED)
find_package(directx12-agility CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_subdirectory(engine/core)
add_subdirectory(engine/rhi)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(engine/renderdata)
add_subdirectory(engine/render)
add_subdirectory(shaders)
add_subdirectory(data)
add_subdirectory(apps/common)
add_subdirectory(apps/sandbox)
add_subdirectory(apps/bench)
add_subdirectory(tests)
