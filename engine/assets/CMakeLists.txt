# The asset library: model and image import, and the cache, which reaches the GPU only through the renderer's
# uploader.

add_library(engine_assets STATIC
        AssetCache.cpp
        AssetCache.h
        ModelImporter.cpp
        ModelImporter.h
        Paths.cpp
        Paths.h
        Primitives.cpp
        Primitives.h
        TextureLoader.cpp
        TextureLoader.h)

engine_target_defaults(engine_assets)

target_include_directories(engine_assets PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_assets
        PUBLIC
        engine_core
        engine_renderdata
        PRIVATE
        Microsoft::DirectXTex
        assimp::assimp)
