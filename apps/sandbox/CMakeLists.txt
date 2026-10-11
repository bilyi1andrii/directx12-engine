# WIN32 => GUI subsystem (wWinMain, no console)
add_executable(sandbox WIN32
        main.cpp
        Helpers.h)

engine_target_defaults(sandbox)

target_link_libraries(sandbox PRIVATE
        Microsoft::DirectX-Headers      # <directx/d3d12.h>, <directx/d3dx12.h> matching the Agility SDK
        Microsoft::DirectX-Guids        # IIDs for interfaces newer than the Windows SDK's dxguid.lib
        d3d12 dxgi dxguid)

add_dependencies(sandbox compile_shaders)
engine_copy_agility_runtime(sandbox)
