# engine_assets: CPU side of asset loading (TextureLoader; later ModelImporter, AssetCache).
# Must never link engine_rhi, d3d12 or dxgi.

add_library(engine_assets STATIC
        TextureLoader.cpp
        TextureLoader.h)

engine_target_defaults(engine_assets)

target_include_directories(engine_assets PUBLIC "${PROJECT_SOURCE_DIR}/engine")

target_link_libraries(engine_assets
        PUBLIC
        engine_core
        # PRIVATE: DirectXTex.h is used only in .cpp files
        PRIVATE
        Microsoft::DirectXTex)
