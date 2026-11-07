# engine_assets: CPU side of asset loading (TextureLoader, ModelImporter; later AssetCache).
# Must never link engine_rhi, d3d12 or dxgi.

add_library(engine_assets STATIC
        ModelImporter.cpp
        ModelImporter.h
        TextureLoader.cpp
        TextureLoader.h)

engine_target_defaults(engine_assets)

target_include_directories(engine_assets PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_assets
        PUBLIC
        engine_core
        # TextureLoader.h returns render::TextureData
        engine_renderdata
        # PRIVATE: DirectXTex and Assimp headers are used only in .cpp files
        PRIVATE
        Microsoft::DirectXTex
        assimp::assimp)
