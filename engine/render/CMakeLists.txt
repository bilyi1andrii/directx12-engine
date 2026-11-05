# engine_render: drives the frame on top of engine_rhi (Renderer, RenderList; later FrameGraph, passes)

add_library(engine_render STATIC
        Handles.h
        Renderer.cpp
        Renderer.h
        RendererSettings.h
        RenderList.cpp
        RenderList.h
        TextureUpload.h)

engine_target_defaults(engine_render)

# PUBLIC: whoever links engine_render can include its headers as "render/Renderer.h"
target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# PRIVATE: Renderer.cpp includes "common/shared_types.hlsli", the structs shared with the shaders
target_include_directories(engine_render PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

# PUBLIC: Renderer.h holds rhi objects as members, so users need the rhi headers too
target_link_libraries(engine_render PUBLIC
        engine_rhi)