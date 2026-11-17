# engine_render: draws the frame with engine_rhi (Renderer, GpuResources, UiRenderer; later FrameGraph, passes)

add_library(engine_render STATIC
        DynamicInstances.cpp
        DynamicInstances.h
        GpuResources.cpp
        GpuResources.h
        InstanceUpload.h
        Renderer.cpp
        Renderer.h
        UiRenderer.cpp
        UiRenderer.h
        passes/OpaquePass.cpp
        passes/OpaquePass.h)

engine_target_defaults(engine_render)

# PUBLIC: whoever links engine_render can include its headers as "render/Renderer.h"
target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# PRIVATE: the .cpp files and InstanceUpload.h include "common/shared_types.hlsli", the structs shared with the
# shaders
target_include_directories(engine_render PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

# PUBLIC: Renderer.h and GpuResources.h use rhi and renderdata types, so users need both
target_link_libraries(engine_render PUBLIC
        engine_renderdata
        engine_rhi)
# PRIVATE: only UiRenderer.cpp includes ImGui (its dx12 backend); Renderer.h just forward-declares ImDrawData
target_link_libraries(engine_render PRIVATE
        imgui::imgui)