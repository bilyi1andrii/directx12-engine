# engine_render: drives the frame on top of engine_rhi (Renderer; later RenderList, FrameGraph, passes)

add_library(engine_render STATIC
        Renderer.cpp
        Renderer.h)

engine_target_defaults(engine_render)

# PUBLIC: whoever links engine_render can include its headers as "render/Renderer.h"
target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")

# PUBLIC: Renderer.h holds rhi objects as members, so users need the rhi headers too
target_link_libraries(engine_render PUBLIC
        engine_rhi)