add_executable(sandbox
        main.cpp)

# Equivalent of WIN32
set_target_properties(sandbox PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

# Start console program in wWinMain (needed to show debugger outputs in terminal)
target_link_options(sandbox PRIVATE
        $<$<NOT:$<CONFIG:Release>>:/ENTRY:wWinMainCRTStartup>)

engine_target_defaults(sandbox)

target_link_libraries(sandbox PRIVATE
        engine_core
        Microsoft::DirectX-Headers      # <directx/d3d12.h>, <directx/d3dx12.h> matching the Agility SDK
        Microsoft::DirectX-Guids        # IIDs for interfaces newer than the Windows SDK's dxguid.lib
        d3d12 dxgi dxguid)

add_dependencies(sandbox compile_shaders)
engine_copy_agility_runtime(sandbox)
