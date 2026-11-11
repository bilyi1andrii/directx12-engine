# engine_renderdata: plain CPU data the renderer draws from (RenderList, handles, settings, texture, mesh and
# material data, lights). Scene, assets and UI link this instead of engine_render. Namespace engine::render.
# Must never link engine_rhi, d3d12 or dxgi.

add_library(engine_renderdata STATIC
        FrameView.h
        GpuUploader.h
        GridLayout.cpp
        GridLayout.h
        Handles.h
        InstanceChunks.cpp
        InstanceChunks.h
        LightPacking.cpp
        LightPacking.h
        Lights.h
        MaterialDesc.h
        MeshData.h
        Model.h
        ModelData.h
        RendererSettings.h
        RenderList.cpp
        RenderList.h
        TextureData.h
        Vertex.h)

engine_target_defaults(engine_renderdata)

# PUBLIC: users include its headers as "renderdata/RenderList.h"
target_include_directories(engine_renderdata PUBLIC "${PROJECT_SOURCE_DIR}/engine")
# PRIVATE: LightPacking.cpp includes "common/shared_types.hlsli", the structs shared with the shaders. That file
# needs only DirectXMath, so this stays CPU code.
target_include_directories(engine_renderdata PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

target_link_libraries(engine_renderdata PUBLIC
        engine_core)
