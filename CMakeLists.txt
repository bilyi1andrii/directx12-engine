cmake_minimum_required(VERSION 3.24)
project(D3D12_Engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every executable lands in build/<preset>/bin, next to the shared D3D12/ (Agility runtime)
# and shaders/ folders, so all apps (sandbox, later bench) find them the same way.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(EngineTargets)

# Packages come from vcpkg.json; the toolchain file is set by CMakePresets.json.
find_package(directx-headers CONFIG REQUIRED)
find_package(directx12-agility CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)

add_subdirectory(engine/core)
add_subdirectory(engine/rhi)
add_subdirectory(shaders)
add_subdirectory(apps/sandbox)
