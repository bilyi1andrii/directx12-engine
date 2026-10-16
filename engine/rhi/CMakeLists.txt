# engine_rhi: thin wrapper over D3D12 and DXGI (device, queues, swap chain, descriptors, ...)

add_library(engine_rhi STATIC
        AgilitySdk.h
        Buffer.cpp
        Buffer.h
        CommandContext.cpp
        CommandContext.h
        CommandQueue.cpp
        CommandQueue.h
        DepthBuffer.cpp
        DepthBuffer.h
        DescriptorAllocator.cpp
        DescriptorAllocator.h
        Device.cpp
        Device.h
        FrameContext.cpp
        FrameContext.h
        RhiCommon.h
        SwapChain.cpp
        SwapChain.h)

engine_target_defaults(engine_rhi)

# PUBLIC: whoever links engine_rhi can include its headers as "rhi/Device.h, ..."
target_include_directories(engine_rhi PUBLIC "${PROJECT_SOURCE_DIR}/engine")

# PUBLIC: rhi headers include <directx/d3d12.h> and <dxgi1_6.h>, so users need the same headers and libraries
target_link_libraries(engine_rhi PUBLIC
        engine_core
        Microsoft::DirectX-Headers      # <directx/d3d12.h>, <directx/d3dx12.h> matching the Agility SDK
        Microsoft::DirectX-Guids        # IIDs for interfaces newer than the Windows SDK's dxguid.lib
        d3d12 dxgi dxguid)