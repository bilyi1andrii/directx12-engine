# engine_framegraph: the CPU-only part of the frame graph (which passes run, shared transient textures, the state
# changes between passes). Must never link engine_rhi, d3d12 or dxgi, so engine_tests can test it without a GPU.

add_library(engine_framegraph STATIC
        FrameGraphCompiler.cpp
        FrameGraphCompiler.h)

engine_target_defaults(engine_framegraph)

# PUBLIC: users include its header as "render/FrameGraphCompiler.h"
target_include_directories(engine_framegraph PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_framegraph PUBLIC
        engine_core)

# engine_render: draws the frame with engine_rhi (Renderer, GpuResources, FrameGraph and its passes, UiRenderer)

add_library(engine_render STATIC
        DynamicInstances.cpp
        DynamicInstances.h
        FrameGraph.cpp
        FrameGraph.h
        FrameInputs.h
        GpuResources.cpp
        GpuResources.h
        InstanceUpload.h
        Renderer.cpp
        Renderer.h
        UiRenderer.cpp
        UiRenderer.h
        passes/OpaquePass.cpp
        passes/OpaquePass.h
        passes/UiPass.cpp
        passes/UiPass.h)

engine_target_defaults(engine_render)

# PUBLIC: whoever links engine_render can include its headers as "render/Renderer.h"
target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# PRIVATE: the .cpp files and InstanceUpload.h include "common/shared_types.hlsli", the structs shared with the
# shaders
target_include_directories(engine_render PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

# PUBLIC: Renderer.h and GpuResources.h use rhi and renderdata types, and FrameGraph.h engine_framegraph's
target_link_libraries(engine_render PUBLIC
        engine_framegraph
        engine_renderdata
        engine_rhi)
# PRIVATE: only UiRenderer.cpp includes ImGui (its dx12 backend); Renderer.h just forward-declares ImDrawData
target_link_libraries(engine_render PRIVATE
        imgui::imgui)