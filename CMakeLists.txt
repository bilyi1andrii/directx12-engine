cmake_minimum_required(VERSION 3.24)

# Builds the engine, its shaders, the sandbox, the bench and the unit tests.

project(D3D12_Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Must be in the top-level file, before the subdirectories that add tests
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(EngineTargets)

find_package(directx-headers CONFIG REQUIRED)
find_package(directx12-agility CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
# Its target does not link imgui: users link both
find_package(imguizmo CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_subdirectory(engine/core)
add_subdirectory(engine/rhi)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(engine/renderdata)
add_subdirectory(engine/render)
add_subdirectory(engine/ui)
add_subdirectory(shaders)
add_subdirectory(data)
add_subdirectory(apps/common)
add_subdirectory(apps/sandbox)
add_subdirectory(apps/bench)
add_subdirectory(tests)
