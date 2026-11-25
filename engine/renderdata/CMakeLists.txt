# The renderer's plain CPU data: what the scene, the assets and the UI hand to the renderer, with no D3D12 in it.

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
        Material.h
        MaterialDesc.h
        MeshData.h
        Model.cpp
        Model.h
        ModelData.h
        RendererSettings.h
        RenderList.cpp
        RenderList.h
        ShadowView.cpp
        ShadowView.h
        TextureData.h
        TransparentSort.cpp
        TransparentSort.h
        Vertex.h)

engine_target_defaults(engine_renderdata)

target_include_directories(engine_renderdata PUBLIC "${PROJECT_SOURCE_DIR}/engine")
target_include_directories(engine_renderdata PRIVATE "${PROJECT_SOURCE_DIR}/shaders")

target_link_libraries(engine_renderdata PUBLIC
        engine_core)
