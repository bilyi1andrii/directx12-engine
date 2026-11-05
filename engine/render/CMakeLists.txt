# engine_render: draws the frame with engine_rhi (Renderer; later FrameGraph, passes)

add_library(engine_render STATIC
        Renderer.cpp
        Renderer.h)

engine_target_defaults(engine_render)

# PUBLIC: whoever links engine_render can include its headers as "render/Renderer.h"
target_include_directories(engine_render PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# PRIVATE: Renderer.cpp includes "common/shared_types.hlsli", the structs shared with the shaders
target_include_directories(engine_render PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

# PUBLIC: Renderer.h uses rhi and renderdata types, so users need both
target_link_libraries(engine_render PUBLIC
        engine_renderdata
        engine_rhi)